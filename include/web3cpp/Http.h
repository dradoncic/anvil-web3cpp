#ifndef HTTP_TRANSPORT_H
#define HTTP_TRANSPORT_H

#include <future>
#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include <web3cpp/Transport.h>

using json = nlohmann::ordered_json;

namespace Net{
/**
 * HTTP/HTTPS transport for making one-shot requests.
 * Creates a new connection for each request, no persistent connection.
 * Suitable for traditional HTTP request-response patterns.
 */
class HttpTransport : public NetworkTransport {
private:
    std::string host;           ///< The remote host.
    std::string port;           ///< The remote port.
    std::string target;         ///< The RPC endpoint target (e.g., "/rpc").
    std::string protocol;       ///< "http" or "https".
    bool connected = true;     ///< Connection state (always false for HTTP).

public:
    /**
     * Constructor.
     * @param _host The remote host.
     * @param _port The remote port.
     * @param _target The RPC endpoint target.
     * @param _protocol "http" or "https".
     */
    HttpTransport(
        const std::string& _host,
        const std::string& _port,
        const std::string& _target,
        const std::string& _protocol
    );

    virtual ~HttpTransport() = default;

    /**
     * Send a JSON-RPC request via HTTP POST.
     * @param request The JSON-RPC request object.
     * @return A future that resolves to the JSON-RPC response.
     */
    std::future<json> send(Net::RequestTypes reqType, const std::string& reqBody) override;

    /**
     * Establish connection (no-op for HTTP).
     * @return A future that completes immediately.
     */
    std::future<void> connect() override;

    /**
     * Check connection status.
     * @return Always returns true for HTTP (stateless).
     */
    bool isConnected() const override;

    /**
     * Disconnect (no-op for HTTP).
     */
    void disconnect() override;

    /**
     * Get the protocol type.
     * @return "http" or "https".
     */
    std::string getProtocol() const override;
};

} // namespace Net

#endif  // HTTP_TRANSPORT_H
