#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>

namespace p2p {

class Message;
class PeerManager;

class NetworkManager {
public:
    using MessageHandler = std::function<void(const std::string& peerId, 
                                            const Message& message)>;
    using ConnectionHandler = std::function<void(const std::string& peerId, bool connected)>;

    NetworkManager(boost::asio::io_context& ioContext, PeerManager& peerManager);
    ~NetworkManager();

    void Start(uint16_t port);
    void Stop();

    void ConnectToPeer(const std::string& address, uint16_t port);
    void DisconnectPeer(const std::string& peerId);
    
    void SendMessage(const std::string& peerId, const Message& message);
    void BroadcastMessage(const Message& message);

    void SetMessageHandler(MessageHandler handler);
    void SetConnectionHandler(ConnectionHandler handler);

    std::vector<std::string> GetConnectedPeers() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace p2p