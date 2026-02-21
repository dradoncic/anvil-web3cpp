#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <future>
#include <string>
#include <cstdlib>
#include <iostream>
#include <string>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <nlohmann/json.hpp>

#include <web3cpp/RequestTypes.h>
#include <web3cpp/Utils.h>
#include <web3cpp/Error.h>

using json = nlohmann::ordered_json;

namespace Net {

class NetworkTransport {
public:
    virtual ~NetworkTransport() = default;

    /**
     * Send a JSON-RPC request and receive a JSON response.
     * @param request The JSON-RPC request object.
     * @return A future that resolves to the JSON-RPC response.
     */
    virtual std::future<json> send(Net::RequestTypes reqType, const std::string& reqBody) = 0;

    /**
     * Establish connection to the remote endpoint.
     * For HTTP: this may be a no-op or warm-up.
     * For WebSocket: this initiates the connection handshake.
     * @return A future that completes when connection is established.
     */
    virtual std::future<void> connect() = 0;

    /**
     * Check if the transport is currently connected/ready.
     * @return true if ready to send requests, false otherwise.
     */
    virtual bool isConnected() const = 0;

    /**
     * Gracefully disconnect from the remote endpoint.
     * For WebSocket: closes the connection.
     * For HTTP: may be a no-op.
     */
    virtual void disconnect() = 0;

    /**
     * Get the protocol type as a string.
     * @return "http", "https", "ws", or "wss".
     */
    virtual std::string getProtocol() const = 0;
};

} // namespace Net

#endif  // TRANSPORT_H
