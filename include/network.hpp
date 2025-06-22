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

    void start(uint16_t port);
    void stop();

    void connectToPeer(const std::string& address, uint16_t port);
    void disconnectPeer(const std::string& peerId);
    
    void sendMessage(const std::string& peerId, const Message& message);
    void broadcastMessage(const Message& message);

    void setMessageHandler(MessageHandler handler);
    void setConnectionHandler(ConnectionHandler handler);

    std::vector<std::string> getConnectedPeers() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace p2p