// #ifndef WEBSOCKET_TRANSPORT_H
// #define WEBSOCKET_TRANSPORT_H

// #include <future>
// #include <string>
// #include <memory>
// #include <thread>
// #include <map>
// #include <mutex>
// #include <atomic>
// #include <condition_variable>
// #include <nlohmann/json.hpp>
// #include <boost/asio.hpp>
// #include <web3cpp/Transport.h>

// using json = nlohmann::ordered_json;
// namespace asio = boost::asio;

// // Forward declaration
// class WebSocketConnection;

// namespace Net{

// /**
//  * WebSocket transport for persistent, long-lived connections.
//  * Maintains a single WebSocket connection and multiplexes multiple concurrent requests.
//  * Uses atomic request ID management to correlate responses with requests.
//  */
// class WebSocketTransport : public NetworkTransport {
// private:
//     std::string host;           ///< The remote host.
//     std::string port;           ///< The remote port.
//     std::string target;         ///< The RPC endpoint target (e.g., "/rpc").
//     std::string protocol;       ///< "ws" or "wss".

//     // I/O Context and threading
//     std::unique_ptr<asio::io_context> ioc;                    ///< Boost ASIO I/O context.
//     std::thread ioThread;                                     ///< Thread that runs ioc.run().

//     // WebSocket connection
//     std::unique_ptr<WebSocketConnection> connection;          ///< The underlying WebSocket connection.

//     // RPC ID management (atomic for thread-safety)
//     std::atomic<uint64_t> rpcCallId{1};                       ///< Auto-incrementing RPC request ID.

//     // Request-response pairing
//     std::map<uint64_t, std::promise<json>> pendingRequests;   ///< Maps RPC ID to response promise.
//     mutable std::mutex pendingRequestsMutex;                  ///< Protects pendingRequests map.

//     // Connection state
//     std::atomic<bool> isConnectedFlag{false};                 ///< Whether the WebSocket is connected.
//     std::condition_variable connectionCv;                     ///< Notified when connection state changes.
//     mutable std::mutex connectionMutex;                       ///< Protects connection state changes.

// public:
//     /**
//      * Constructor.
//      * @param _host The remote host.
//      * @param _port The remote port.
//      * @param _target The RPC endpoint target.
//      * @param _protocol "ws" or "wss".
//      */
//     WebSocketTransport(
//         const std::string& _host,
//         const std::string& _port,
//         const std::string& _target,
//         const std::string& _protocol
//     );

//     /**
//      * Destructor.
//      * Cleanly shuts down the I/O context and joins the thread.
//      */
//     virtual ~WebSocketTransport();

//     /**
//      * Send a JSON-RPC request via WebSocket.
//      * Automatically assigns the next available RPC ID.
//      * @param request The JSON-RPC request object (will have "id" field set).
//      * @return A future that resolves to the JSON-RPC response.
//      */
//     std::future<std::string> send(const std::string& reqBody, Net::RequestTypes reqType) override;

//     /**
//      * Establish WebSocket connection to the remote endpoint.
//      * This is an async operation that returns before the connection is fully established.
//      * @return A future that completes when the connection handshake is done.
//      */
//     std::future<void> connect() override;

//     /**
//      * Check if the WebSocket is currently connected.
//      * @return true if connected and ready to send, false otherwise.
//      */
//     bool isConnected() const override;

//     /**
//      * Gracefully close the WebSocket connection.
//      * Pending requests will fail.
//      */
//     void disconnect() override;

//     /**
//      * Get the protocol type.
//      * @return "ws" or "wss".
//      */
//     std::string getProtocol() const override;

// private:
//     /**
//      * Allocate the next RPC request ID.
//      * Thread-safe via atomic operation.
//      * @return The next available RPC ID.
//      */
//     uint64_t getNextRequestId();

//     /**
//      * Called by the WebSocket connection when a complete message is received.
//      * Parses the JSON response, extracts the ID, and resolves the corresponding promise.
//      * @param message The complete message received from the server.
//      */
//     void onMessageReceived(const std::string& message);

//     /**
//      * Called by the WebSocket connection when the connection is established.
//      */
//     void onConnected();

//     /**
//      * Called by the WebSocket connection when the connection is closed.
//      * @param reason The reason for closure (for logging/debugging).
//      */
//     void onDisconnected(const std::string& reason);

//     /**
//      * Called by the WebSocket connection when an error occurs.
//      * @param error The error message.
//      */
//     void onError(const std::string& error);

//     // Grant access to WebSocketConnection for callbacks
//     friend class WebSocketConnection;
// };

// }

// #endif  // WEBSOCKET_TRANSPORT_H
