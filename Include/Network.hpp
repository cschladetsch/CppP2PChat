#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <thread>

namespace p2p {

class Message;
class PeerManager;

class NetworkManager {
public:
    using MessageHandler = std::function<void(const std::string& peerId, 
                                            const Message& message)>;
    using ConnectionHandler = std::function<void(const std::string& peerId, bool connected)>;

    NetworkManager(PeerManager& peerManager);
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
    std::unique_ptr<Impl> pImpl_;
};

} // namespace p2p