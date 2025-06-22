#include "Network.hpp"
#include "Message.hpp"
#include "PeerManager.hpp"
#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <iostream>

namespace p2p {

using boost::asio::ip::tcp;
using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable;

class Session : public std::enable_shared_from_this<Session> {
public:
    using InternalMessageHandler = std::function<void(std::shared_ptr<Session>, const Message&)>;
    
    Session(tcp::socket socket, InternalMessageHandler handler,
            NetworkManager::ConnectionHandler& connHandler, bool isOutgoing = false)
        : socket_(std::move(socket))
        , messageHandler_(handler)
        , connectionHandler_(connHandler)
        , isOutgoing_(isOutgoing) {}

    awaitable<void> Start() {
        try {
            co_await Run();
        } catch (const std::exception& e) {
            handleError();
        }
    }

    awaitable<void> SendMessage(const Message& message) {
        try {
            auto data = message.Serialize();
            co_await boost::asio::async_write(socket_,
                boost::asio::buffer(data), use_awaitable);
        } catch (...) {
            // Socket error during send
        }
    }

    const std::string& GetPeerId() const { return peerId_; }
    void SetPeerId(const std::string& id) { peerId_ = id; }
    bool HasPeerId() const { return !peerId_.empty(); }
    bool IsOutgoing() const { return isOutgoing_; }
    
    tcp::endpoint GetRemoteEndpoint() const {
        return socket_.remote_endpoint();
    }
    
    tcp::socket& GetSocket() { return socket_; }

private:
    awaitable<void> Run() {
        while (true) {
            // Read header
            std::vector<uint8_t> headerBuffer(13);
            co_await boost::asio::async_read(socket_,
                boost::asio::buffer(headerBuffer), use_awaitable);
            
            // Parse payload size
            uint32_t payloadSize = (static_cast<uint32_t>(headerBuffer[1]) << 24) |
                                 (static_cast<uint32_t>(headerBuffer[2]) << 16) |
                                 (static_cast<uint32_t>(headerBuffer[3]) << 8) |
                                 static_cast<uint32_t>(headerBuffer[4]);
            
            if (payloadSize > 1024 * 1024 * 10) { // 10MB limit
                throw std::runtime_error("Message too large");
            }
            
            // Read full message
            std::vector<uint8_t> fullMessage(headerBuffer);
            fullMessage.resize(13 + payloadSize);
            co_await boost::asio::async_read(socket_,
                boost::asio::buffer(fullMessage.data() + 13, payloadSize),
                use_awaitable);
            
            // Process message
            Message msg = Message::Deserialize(fullMessage);
            if (messageHandler_) {
                messageHandler_(shared_from_this(), msg);
            }
        }
    }

    void handleError() {
        if (connectionHandler_ && HasPeerId()) {
            connectionHandler_(peerId_, false);
        }
        socket_.close();
    }

    tcp::socket socket_;
    InternalMessageHandler messageHandler_;
    NetworkManager::ConnectionHandler& connectionHandler_;
    std::string peerId_;
    bool isOutgoing_;
};

struct NetworkManager::Impl {
    boost::asio::io_context& ioContext_;
    PeerManager& peerManager_;
    std::unique_ptr<tcp::acceptor> acceptor_;
    MessageHandler userMessageHandler_;
    ConnectionHandler connectionHandler_;
    std::unordered_map<std::string, std::shared_ptr<Session>> sessions_;
    std::mutex sessionsMutex_;
    bool running_ = false;

    Impl(boost::asio::io_context& io, PeerManager& pm) 
        : ioContext_(io), peerManager_(pm) {}
    
    void HandleSessionMessage(std::shared_ptr<Session> session, const Message& msg) {
        if (msg.GetType() == MessageType::HANDSHAKE && !session->HasPeerId()) {
            auto payload = msg.GetPayload();
            if (payload.size() >= 2) {
                uint16_t idLen = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];
                if (payload.size() >= static_cast<size_t>(2 + idLen)) {
                    std::string peerId(payload.begin() + 2, payload.begin() + 2 + idLen);
                    std::vector<uint8_t> publicKey(payload.begin() + 2 + idLen, payload.end());
                    
                    // Set peer ID on session
                    session->SetPeerId(peerId);
                    
                    // Add to sessions map
                    {
                        std::lock_guard<std::mutex> lock(sessionsMutex_);
                        sessions_[peerId] = session;
                    }
                    
                    // Update peer info
                    try {
                        auto endpoint = session->GetRemoteEndpoint();
                        PeerInfo peer;
                        peer.id = peerId;
                        peer.publicKey = publicKey;
                        peer.address = endpoint.address().to_string();
                        peer.port = endpoint.port();
                        peer.isConnected = true;
                        peer.lastSeen = std::chrono::system_clock::now();
                        peerManager_.AddPeer(peer);
                    } catch (...) {}
                    
                    // Notify connection
                    if (connectionHandler_) {
                        connectionHandler_(peerId, true);
                    }
                    
                    // Send handshake response only for incoming connections
                    if (!session->IsOutgoing()) {
                        auto localPeer = peerManager_.GetLocalPeer();
                        auto response = Message::CreateHandshakeMessage(localPeer.id, localPeer.publicKey);
                        co_spawn(ioContext_,
                            session->SendMessage(response), detached);
                    }
                }
            }
        }
        
        // Forward to user handler if session has peer ID
        if (session->HasPeerId() && userMessageHandler_) {
            userMessageHandler_(session->GetPeerId(), msg);
        }
    }

    awaitable<void> AcceptLoop() {
        while (running_ && acceptor_ && acceptor_->is_open()) {
            try {
                tcp::socket socket = co_await acceptor_->async_accept(use_awaitable);
                
                auto session = std::make_shared<Session>(
                    std::move(socket), 
                    [this](std::shared_ptr<Session> s, const Message& m) {
                        HandleSessionMessage(s, m);
                    }, 
                    connectionHandler_);
                    
                co_spawn(acceptor_->get_executor(),
                    session->Start(), detached);
            } catch (const std::exception& e) {
                // Accept error, continue if still running
            }
        }
    }
};

NetworkManager::NetworkManager(boost::asio::io_context& ioContext, PeerManager& peerManager)
    : pImpl_(std::make_unique<Impl>(ioContext, peerManager)) {}

NetworkManager::~NetworkManager() {
    Stop();
}

void NetworkManager::Start(uint16_t port) {
    pImpl_->acceptor_ = std::make_unique<tcp::acceptor>(pImpl_->ioContext_, tcp::endpoint(tcp::v4(), port));
    pImpl_->running_ = true;
    co_spawn(pImpl_->ioContext_,
        pImpl_->AcceptLoop(), detached);
}

void NetworkManager::Stop() {
    pImpl_->running_ = false;
    if (pImpl_->acceptor_) {
        pImpl_->acceptor_->close();
    }
    
    // Close all sessions first
    {
        std::lock_guard<std::mutex> lock(pImpl_->sessionsMutex_);
        for (auto& [id, session] : pImpl_->sessions_) {
            if (session) {
                try {
                    session->GetSocket().close();
                } catch (...) {}
            }
        }
        pImpl_->sessions_.clear();
    }
}

void NetworkManager::ConnectToPeer(const std::string& address, uint16_t port) {
    auto impl = pImpl_.get();
    co_spawn(pImpl_->ioContext_,
        [impl, address, port]() -> awaitable<void> {
            try {
                tcp::resolver resolver(impl->ioContext_);
                auto endpoints = co_await resolver.async_resolve(
                    address, std::to_string(port), use_awaitable);
                
                tcp::socket socket(impl->ioContext_);
                co_await boost::asio::async_connect(socket, endpoints, use_awaitable);
                
                auto session = std::make_shared<Session>(
                    std::move(socket), 
                    [impl](std::shared_ptr<Session> s, const Message& m) {
                        impl->HandleSessionMessage(s, m);
                    }, 
                    impl->connectionHandler_,
                    true);  // isOutgoing = true
                    
                // Send handshake
                auto localPeer = impl->peerManager_.GetLocalPeer();
                auto handshake = Message::CreateHandshakeMessage(localPeer.id, localPeer.publicKey);
                co_await session->SendMessage(handshake);
                
                // Start reading
                co_await session->Start();
            } catch (const std::exception& e) {
                // Connection failed
            }
        }(), detached);
}

void NetworkManager::DisconnectPeer(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(pImpl_->sessionsMutex_);
    auto it = pImpl_->sessions_.find(peerId);
    if (it != pImpl_->sessions_.end()) {
        pImpl_->sessions_.erase(it);
        if (pImpl_->connectionHandler_) {
            pImpl_->connectionHandler_(peerId, false);
        }
    }
}

void NetworkManager::SendMessage(const std::string& peerId, const Message& message) {
    std::shared_ptr<Session> session;
    {
        std::lock_guard<std::mutex> lock(pImpl_->sessionsMutex_);
        auto it = pImpl_->sessions_.find(peerId);
        if (it != pImpl_->sessions_.end()) {
            session = it->second;
        }
    }
    if (session) {
        co_spawn(pImpl_->ioContext_,
            session->SendMessage(message), detached);
    }
}

void NetworkManager::BroadcastMessage(const Message& message) {
    std::vector<std::shared_ptr<Session>> sessions;
    {
        std::lock_guard<std::mutex> lock(pImpl_->sessionsMutex_);
        for (const auto& [peerId, session] : pImpl_->sessions_) {
            sessions.push_back(session);
        }
    }
    for (auto& session : sessions) {
        co_spawn(pImpl_->ioContext_,
            session->SendMessage(message), detached);
    }
}

void NetworkManager::SetMessageHandler(MessageHandler handler) {
    pImpl_->userMessageHandler_ = handler;
}

void NetworkManager::SetConnectionHandler(ConnectionHandler handler) {
    pImpl_->connectionHandler_ = handler;
}

std::vector<std::string> NetworkManager::GetConnectedPeers() const {
    std::lock_guard<std::mutex> lock(pImpl_->sessionsMutex_);
    std::vector<std::string> peers;
    for (const auto& [peerId, session] : pImpl_->sessions_) {
        peers.push_back(peerId);
    }
    return peers;
}

} // namespace p2p