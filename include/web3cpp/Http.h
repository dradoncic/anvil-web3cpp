#ifndef HTTP_TRANSPORT_H
#define HTTP_TRANSPORT_H

#include <web3cpp/Transport.h>

#include <future>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::ordered_json;

namespace Net
{

// Creates a new connection for each request, no persistent connection.
class HttpTransport : public NetworkTransport
{
 private:
  std::string host;
  std::string port;
  std::string target;
  std::string protocol;
  bool connected = true;

 public:
  /**
   * @param _host The remote host.
   * @param _port The remote port.
   * @param _target The RPC endpoint target.
   * @param _protocol "http" or "https".
   */
  HttpTransport(const std::string& _host, const std::string& _port, const std::string& _target,
                const std::string& _protocol);

  virtual ~HttpTransport() = default;

  /**
   * @brief Send a JSON-RPC request via HTTP POST.
   * @param request The JSON-RPC request object.
   * @return A future that resolves to the JSON-RPC response.
   */
  std::future<json> send(Net::RequestTypes reqType, const std::string& reqBody) override;

  /**
   *@brief Establish connection (no-op for HTTP).
   * @return A future that completes immediately.
   */
  std::future<void> connect() override;

  /**
   * @briefCheck connection status.
   * @return Always returns true for HTTP (stateless).
   */
  bool isConnected() const override;

  /**
   * @brief Disconnect (no-op for HTTP).
   */
  void disconnect() override;

  /**
   * @brief Get the protocol type.
   * @return "http" or "https".
   */
  std::string getProtocol() const override;
};

}  // namespace Net

#endif  // HTTP_TRANSPORT_H
