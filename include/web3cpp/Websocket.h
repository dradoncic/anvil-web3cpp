#ifndef WEBSOCKET_TRANSPORT_H
#define WEBSOCKET_TRANSPORT_H

#include <web3cpp/Transport.h>

#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <deque>
#include <future>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <thread>

using json = nlohmann::ordered_json;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ip = boost::asio::ip;

namespace Net
{

/// WebSocket transport for persistent, long-lived connections.
class WebSocketTransport : public NetworkTransport
{
 public:
  /**
   * @param _host     The remote host.
   * @param _port     The remote port.
   * @param _target   The RPC endpoint target.
   * @param _protocol "ws" or "wss".
   */
  WebSocketTransport(const std::string& host, const std::string& port, const std::string& target,
                     const std::string& protocol);

  /// Cleanly shuts down the I/O context and joins the thread.
  ~WebSocketTransport() override;

  /// @brief Send a JSON-RPC request via WebSocket.
  /// @param reqType  The type of request being sent.
  /// @param reqBody  The serialised JSON-RPC request body (will have "id" field set).
  /// @return A future that resolves to the JSON-RPC response.
  std::future<json> send(Net::RequestTypes reqType, const std::string& reqBody) override;

  /// @brief Establish WebSocket connection to the remote endpoint.
  /// @return A future that completes when the connection handshake is done.
  std::future<void> connect() override;

  /// @brief Check if the WebSocket is currently connected.
  bool isConnected() const override;

  /// @brief Gracefully close the WebSocket connection.
  void disconnect() override;

  /// @brief Get the protocol type ("ws" or "wss").
  std::string getProtocol() const override;

 private:
  /// Called when the resolver completes.
  void on_resolve(beast::error_code ec, ip::tcp::resolver::results_type results);

  /// Called when the TCP connection completes.
  void on_connect(beast::error_code ec);

  /// Called when the SSL/TLS handshake completes.
  void on_ssl_handshake(beast::error_code ec);

  /// Called when the WebSocket handshake completes.
  void on_handshake(beast::error_code ec);

  /// Called when a read operation completes.
  void on_read(beast::error_code ec, size_t bytes_transferred);

  /// Drain the write queue; sends the next message if one is pending.
  void do_write();

  // Clean up the queue.
  void clean_up();

  /// Process an incoming WebSocket message (dispatches RPC responses).
  void handle_message(std::string_view msg);

  /// Handle an unexpected disconnect (attempts to reconnect via connect()).
  void handle_disconnect();

  std::string host_;
  std::string port_;
  std::string target_;
  std::string protocol_;

  net::io_context ioc_;
  std::thread io_thread_;
  net::ssl::context ssl_ctx_;
  websocket::stream<beast::ssl_stream<ip::tcp::socket>> ws_;
  ip::tcp::resolver resolver_;
  net::executor_work_guard<net::io_context::executor_type> work_guard_;

  beast::flat_buffer read_buffer_;
  std::deque<std::string> write_queue_;
  bool writing_ = false;

  std::atomic<uint64_t> rpc_id_{1};
  std::map<uint64_t, std::promise<json>> pending_requests_;
  mutable std::mutex mtx_pending_;

  std::atomic<bool> connected_{false};
  std::promise<void> connect_promise_;
};

}  // namespace Net

#endif  // WEBSOCKET_TRANSPORT_H
