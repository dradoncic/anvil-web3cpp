#include <future>
#include <string>
#include <cstdlib>
#include <iostream>
#include <string>
#include <web3cpp/Http.h>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/certify/extensions.hpp>
#include <boost/certify/https_verification.hpp>

namespace Net{

HttpTransport::HttpTransport(const std::string& _host,
                                const std::string& _port,
                                const std::string& _target,
                                const std::string& _protocol) :
                            host{_host}, port{_port}, target{_target},
                            protocol{_protocol} {}

std::future<json> HttpTransport::send(Net::RequestTypes reqType, const std::string& reqBody)
{
    return std::async(std::launch::async, [=]() {
        std::string result = "";
        using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
        namespace ssl = boost::asio::ssl;
        namespace http = boost::beast::http;    // from <boost/beast/http.hpp>

        // Uncomment to see request details
        //std::cout << "host: " << host << std::endl;
        //std::cout << "target: " << target << std::endl;
        //std::cout << "port: " << port << std::endl;
        //std::cout << "reqBody: " << reqBody << std::endl;
        auto build_request = [&]() {
            http::request<http::string_body> req{
                (reqType == RequestTypes::POST) ? http::verb::post : http::verb::get,
                target,
                11
            };

            req.set(http::field::host, host);
            req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
            req.set(http::field::content_type, "application/json");

            if (reqType == RequestTypes::POST) {
                req.set(http::field::accept, "application/json");
                req.body() = reqBody;
                req.prepare_payload();
            }

            return req;
        };

        try {
            boost::asio::io_context ioc;
            tcp::resolver resolver(ioc);
            boost::beast::flat_buffer buffer;

            auto const results = resolver.resolve(host, port);

            if (protocol == "https")
            {
                ssl::context ctx{ssl::context::sslv23_client};
                ctx.set_verify_mode(ssl::context::verify_peer | ssl::context::verify_fail_if_no_peer_cert);
                ctx.set_default_verify_paths();
                boost::certify::enable_native_https_server_verification(ctx);

                ssl::stream<tcp::socket> stream{ioc, ctx};

                boost::certify::sni_hostname(stream, host);

                boost::asio::connect(stream.next_layer(), results.begin(), results.end());
                stream.handshake(ssl::stream_base::client);

                auto req = build_request();
                http::write(stream, req);

                http::response<http::dynamic_body> res;
                http::read(stream, buffer, res);

                std::string body {
                    boost::asio::buffers_begin(res.body().data()),
                    boost::asio::buffers_end(res.body().data())
                };

                boost::system::error_code ec;
                stream.shutdown(ec);

                if (ec == boost::asio::error::eof ||
                    ec == boost::asio::ssl::error::stream_truncated)
                {
                    ec.clear();
                }
                if (ec)
                    throw boost::system::system_error{ec};

                return json::parse(body);
            }

            else if (protocol == "http")
            {
                tcp::socket socket{ioc};
                boost::asio::connect(socket, results);

                auto req = build_request();
                http::write(socket, req);

                http::response<http::dynamic_body> res;
                http::read(socket, buffer, res);

                std::string body{
                    boost::asio::buffers_begin(res.body().data()),
                    boost::asio::buffers_end(res.body().data())
                };

                boost::system::error_code ec;
                socket.shutdown(tcp::socket::shutdown_both, ec);
                socket.close(ec);

                return json::parse(body);
            }

            else {
                throw std::runtime_error("Unsupported protocol: " + protocol);
            }

          }
          catch (std::exception const& e) {
              throw std::runtime_error(std::string("HTTP Request error: ") + e.what());
          }
    });
}

std::future<void> HttpTransport::connect() { return std::async(std::launch::async, [](){}); }

bool HttpTransport::isConnected() const { return connected; }

void HttpTransport::disconnect() {};

std::string HttpTransport::getProtocol() const {return protocol; }

} //namespace Net
