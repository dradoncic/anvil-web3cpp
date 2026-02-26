#include "include/web3cpp/Websocket.h"

#include <web3cpp/Websocket.h>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core/role.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <exception>

namespace Net
{

WebSocketTransport::WebSocketTransport(const std::string& host, const std::string& port,
                                       const std::string& target, const std::string& protocol)
    : host_{host}
    , port_{port}
    , target_{target}
    , protocol_{protocol}
    , work_guard_{net::make_work_guard(ioc_)}
    , ws_(ioc_)
    , resolver_(ioc_)
{
  // ssl_ctx.set_options(
  //     net::ssl::context::default_workarounds | net::ssl::context::no_sslv2 |
  //     net::ssl::context::no_sslv3 | net::ssl::context::no_tlsv1 |
  //     net::ssl::context::no_tlsv1_1);

  // ssl_ctx.set_default_verify_paths();
  // ssl_ctx.set_verify_mode(net::ssl::verify_peer);

  // SSL_CTX_set_tlsext_servername_callback(ssl_ctx.native_handle(), nullptr);

  io_thread_ = std::thread([this] { ioc_.run(); });

  connect().get();
}

WebSocketTransport::~WebSocketTransport()
{
  work_guard_.reset();

  if (connected_.load())
  {
    disconnect();
  }

  ioc_.stop();
  if (io_thread_.joinable()) io_thread_.join();
}

std::future<void> WebSocketTransport::connect()
{
  connect_promise_ = std::promise<void>();
  auto fut = connect_promise_.get_future();

  resolver_.async_resolve(host_, port_,
                          [this](beast::error_code ec, net::ip::tcp::resolver::results_type results)
                          { on_resolve(ec, results); });
  return fut;
}

std::future<json> WebSocketTransport::send(Net::RequestTypes reqType, const std::string& reqBody)
{
  auto j = json::parse(reqBody);
  uint64_t id = rpc_id_++;
  j["id"] = id;

  std::promise<json> p;
  auto fut = p.get_future();

  {
    std::lock_guard<std::mutex> lock(mtx_pending_);
    pending_requests_.emplace(id, std::move(p));
  }

  auto msg = j.dump();

  net::post(ioc_,
            [this, msg]
            {
              bool write_idle = write_queue_.empty();
              write_queue_.push_back(msg);

              if (write_idle) do_write();
            });

  return fut;
}

bool WebSocketTransport::isConnected() const
{
  return connected_.load();
}

void WebSocketTransport::disconnect()
{
  if (!connected_.load()) return;

  net::post(ioc_,
            [this]()
            {
              write_queue_.clear();
              writing_ = false;
              ws_.async_close(websocket::close_code::normal,
                              [this](beast::error_code ec)
                              {
                                clean_up();
                                connected_.store(false);
                              });
            });
}

void WebSocketTransport::clean_up()
{
  std::lock_guard<std::mutex> lock(mtx_pending_);
  for (auto& [id, promise] : pending_requests_)
  {
    try
    {
      promise.set_exception(
          std::make_exception_ptr(std::runtime_error("Websocket transport shutting down")));
    }
    catch (...)
    {
    }
  }
  pending_requests_.clear();
}

std::string WebSocketTransport::getProtocol() const
{
  return protocol_;
}

void WebSocketTransport::on_resolve(beast::error_code ec, ip::tcp::resolver::results_type results)
{
  if (ec)
  {
    try
    {
      connect_promise_.set_exception(
          std::make_exception_ptr(std::runtime_error("resolve failed: " + ec.message())));
    }
    catch (...)
    {
    }
    return;
  }

  async_connect(ws_.next_layer(), results.begin(), results.end(),
                [this](beast::error_code ec, auto) { on_connect(ec); });
}

void WebSocketTransport::on_connect(beast::error_code ec)
{
  if (ec)
  {
    try
    {
      connect_promise_.set_exception(
          std::make_exception_ptr(std::runtime_error("tcp connect failed: " + ec.message())));
    }
    catch (...)
    {
    }
    return;
  }
  ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));

  ws_.set_option(websocket::stream_base::decorator(
      [](websocket::request_type& req) {
        req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " web3cpp-ws");
      }));

  auto handshake_host = host_ + ":" + port_;

  ws_.async_handshake(handshake_host, target_, [this](beast::error_code ec) { on_handshake(ec); });
}

void WebSocketTransport::on_handshake(beast::error_code ec)
{
  if (ec)
  {
    try
    {
      connect_promise_.set_exception(
          std::make_exception_ptr(std::runtime_error("ws handshake failed: " + ec.message())));
    }
    catch (...)
    {
    }
    return;
  }

  connected_.store(true);

  try
  {
    connect_promise_.set_value();
  }
  catch (...)
  {
  }

  ws_.async_read(read_buffer_, [this](beast::error_code ec, size_t bytes_transferred)
                 { on_read(ec, bytes_transferred); });
}

void WebSocketTransport::on_read(beast::error_code ec, size_t bytes_transferred)
{
  if (ec)
  {
    connected_.store(false);
    handle_disconnect();
    return;
  }

  auto message = beast::buffers_to_string(read_buffer_.data());
  read_buffer_.consume(bytes_transferred);

  handle_message(message);

  ws_.async_read(read_buffer_, [this](beast::error_code ec, size_t bytes_transferred)
                 { on_read(ec, bytes_transferred); });
}

void WebSocketTransport::do_write()
{
  if (write_queue_.empty())
  {
    writing_ = false;
    return;
  }

  writing_ = true;
  ws_.async_write(net::buffer(write_queue_.front()),
                  [this](beast::error_code ec, size_t bytes_transferred)
                  {
                    if (ec)
                    {
                      std::cerr << "ws write error: " << ec.message() << "\n";
                      writing_ = false;
                      return;
                    }
                    write_queue_.pop_front();
                    do_write();
                  });
}

void WebSocketTransport::handle_message(std::string_view msg)
{
  json j;
  try
  {
    j = json::parse(msg);
  }
  catch (const std::exception& e)
  {
    std::cerr << "ws: failed to parse incoming message: " << e.what() << "\n";
    return;
  }

  if (!j.contains("id") || j["id"].is_null()) return;

  auto id = j["id"].get<uint64_t>();

  std::lock_guard<std::mutex> lock(mtx_pending_);
  auto it = pending_requests_.find(id);
  if (it != pending_requests_.end())
  {
    try
    {
      it->second.set_value(std::move(j));
    }
    catch (...)
    {
    }
    pending_requests_.erase(it);
  }
}

void WebSocketTransport::handle_disconnect()
{
  connected_.store(false);

  std::cerr << "ws: disconnected, attempting reconnect...\n";

  clean_up();
  write_queue_.clear();
  writing_ = false;

  beast::error_code ec;
  ws_.close(websocket::close_code::abnormal);
  ws_.~stream();
  new (&ws_) websocket::stream<ip::tcp::socket>(
      ioc_);

  connect();
}

}  // namespace Net
