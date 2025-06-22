#include "network.hpp"
#include "message.hpp"
#include "peer_manager.hpp"
#include "crypto.hpp"
#include <boost/asio.hpp>
#include <boost/asio/strand.hpp>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <queue>
#include <iostream>

namespace p2p {

using boost::asio::ip::tcp;

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, NetworkManager::MessageHandler& handler,
            NetworkManager::ConnectionHandler& connHandler,
            const std::string& peerId)
        : socket_(std::move(socket))
        , messageHandler_(handler)
        , connectionHandler_(connHandler)
        , peerId_(peerId) {}

    void start() {
        doReadHeader();
    }

    void sendMessage(const Message& message) {
        auto data = message.serialize();
        auto self(shared_from_this());
        
        boost::asio::async_write(socket_,
            boost::asio::buffer(data),
            [this, self](boost::system::error_code ec, std::size_t) {
                if (ec) {
                    handleError(ec);
                }
            });
    }

    const std::string& getPeerId() const { return peerId_; }
    void setPeerId(const std::string& id) { peerId_ = id; }

private:
    void doReadHeader() {
        auto self(shared_from_this());
        headerBuffer_.resize(13);
        
        boost::asio::async_read(socket_,
            boost::asio::buffer(headerBuffer_),
            [this, self](boost::system::error_code ec, std::size_t) {
                if (!ec) {
                    uint32_t payloadSize = (static_cast<uint32_t>(headerBuffer_[1]) << 24) |
                                         (static_cast<uint32_t>(headerBuffer_[2]) << 16) |
                                         (static_cast<uint32_t>(headerBuffer_[3]) << 8) |
                                         static_cast<uint32_t>(headerBuffer_[4]);
                    
                    if (payloadSize > 1024 * 1024 * 10) { // 10MB limit
                        handleError(boost::system::errc::make_error_code(
                            boost::system::errc::message_size));
                        return;
                    }
                    
                    doReadPayload(payloadSize);
                } else {
                    handleError(ec);
                }
            });
    }

    void doReadPayload(uint32_t payloadSize) {
        auto self(shared_from_this());
        std::vector<uint8_t> fullMessage(headerBuffer_);
        fullMessage.resize(13 + payloadSize);
        
        boost::asio::async_read(socket_,
            boost::asio::buffer(fullMessage.data() + 13, payloadSize),
            [this, self, fullMessage](boost::system::error_code ec, std::size_t) {
                if (!ec) {
                    try {
                        Message msg = Message::deserialize(fullMessage);
                        if (messageHandler_) {
                            messageHandler_(peerId_, msg);
                        }
                        doReadHeader();
                    } catch (const std::exception& e) {
                        handleError(boost::system::errc::make_error_code(
                            boost::system::errc::protocol_error));
                    }
                } else {
                    handleError(ec);
                }
            });
    }

    void handleError(const boost::system::error_code& ec) {
        if (connectionHandler_ && !peerId_.empty()) {
            connectionHandler_(peerId_, false);
        }
        socket_.close();
    }

    tcp::socket socket_;
    NetworkManager::MessageHandler& messageHandler_;
    NetworkManager::ConnectionHandler& connectionHandler_;
    std::string peerId_;
    std::vector<uint8_t> headerBuffer_;
};

struct NetworkManager::Impl {
    boost::asio::io_context& ioContext;
    PeerManager& peerManager;
    std::unique_ptr<tcp::acceptor> acceptor;
    MessageHandler messageHandler;
    ConnectionHandler connectionHandler;
    std::unordered_map<std::string, std::shared_ptr<Session>> sessions;
    std::mutex sessionsMutex;
    bool running = false;

    Impl(boost::asio::io_context& io, PeerManager& pm) 
        : ioContext(io), peerManager(pm) {}

    void startAccept() {
        if (!acceptor || !acceptor->is_open()) return;
        
        acceptor->async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    auto session = std::make_shared<Session>(
                        std::move(socket), messageHandler, connectionHandler, "");
                    session->start();
                    
                    // Temporary store until we get peer ID from handshake
                    std::lock_guard<std::mutex> lock(sessionsMutex);
                    sessions["temp_" + std::to_string(sessions.size())] = session;
                }
                if (running) {
                    startAccept();
                }
            });
    }

    void handleConnection(const std::string& peerId, std::shared_ptr<Session> session) {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        
        // Remove temporary entry and add with real peer ID
        for (auto it = sessions.begin(); it != sessions.end(); ++it) {
            if (it->second == session) {
                sessions.erase(it);
                break;
            }
        }
        
        sessions[peerId] = session;
        session->setPeerId(peerId);
        
        if (connectionHandler) {
            connectionHandler(peerId, true);
        }
    }
};

NetworkManager::NetworkManager(boost::asio::io_context& ioContext, PeerManager& peerManager)
    : pImpl(std::make_unique<Impl>(ioContext, peerManager)) {}

NetworkManager::~NetworkManager() {
    stop();
}

void NetworkManager::start(uint16_t port) {
    pImpl->acceptor = std::make_unique<tcp::acceptor>(pImpl->ioContext, tcp::endpoint(tcp::v4(), port));
    pImpl->running = true;
    pImpl->startAccept();
}

void NetworkManager::stop() {
    pImpl->running = false;
    if (pImpl->acceptor) {
        pImpl->acceptor->close();
    }
    
    std::lock_guard<std::mutex> lock(pImpl->sessionsMutex);
    pImpl->sessions.clear();
}

void NetworkManager::connectToPeer(const std::string& address, uint16_t port) {
    auto socket = std::make_shared<tcp::socket>(pImpl->ioContext);
    tcp::resolver resolver(pImpl->ioContext);
    
    auto endpoints = resolver.resolve(address, std::to_string(port));
    
    boost::asio::async_connect(*socket, endpoints,
        [this, socket](boost::system::error_code ec, tcp::endpoint) {
            if (!ec) {
                auto session = std::make_shared<Session>(
                    std::move(*socket), pImpl->messageHandler, pImpl->connectionHandler, "");
                session->start();
                
                // Send handshake
                auto localPeer = pImpl->peerManager.getLocalPeer();
                auto handshake = Message::createHandshakeMessage(localPeer.id, localPeer.publicKey);
                session->sendMessage(handshake);
                
                std::lock_guard<std::mutex> lock(pImpl->sessionsMutex);
                pImpl->sessions["temp_outgoing_" + std::to_string(pImpl->sessions.size())] = session;
            }
        });
}

void NetworkManager::disconnectPeer(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(pImpl->sessionsMutex);
    auto it = pImpl->sessions.find(peerId);
    if (it != pImpl->sessions.end()) {
        pImpl->sessions.erase(it);
        if (pImpl->connectionHandler) {
            pImpl->connectionHandler(peerId, false);
        }
    }
}

void NetworkManager::sendMessage(const std::string& peerId, const Message& message) {
    std::lock_guard<std::mutex> lock(pImpl->sessionsMutex);
    auto it = pImpl->sessions.find(peerId);
    if (it != pImpl->sessions.end()) {
        it->second->sendMessage(message);
    }
}

void NetworkManager::broadcastMessage(const Message& message) {
    std::lock_guard<std::mutex> lock(pImpl->sessionsMutex);
    for (const auto& [peerId, session] : pImpl->sessions) {
        if (!peerId.starts_with("temp_")) {
            session->sendMessage(message);
        }
    }
}

void NetworkManager::setMessageHandler(MessageHandler handler) {
    pImpl->messageHandler = handler;
}

void NetworkManager::setConnectionHandler(ConnectionHandler handler) {
    pImpl->connectionHandler = handler;
}

std::vector<std::string> NetworkManager::getConnectedPeers() const {
    std::lock_guard<std::mutex> lock(pImpl->sessionsMutex);
    std::vector<std::string> peers;
    for (const auto& [peerId, session] : pImpl->sessions) {
        if (!peerId.starts_with("temp_")) {
            peers.push_back(peerId);
        }
    }
    return peers;
}

} // namespace p2p