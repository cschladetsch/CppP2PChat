#include "Network.hpp"
#include "Message.hpp"
#include "PeerManager.hpp"
#include <zmq.hpp>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <iostream>
#include <sstream>

namespace p2p {

struct NetworkManager::Impl {
    PeerManager& peerManager_;
    MessageHandler userMessageHandler_;
    ConnectionHandler connectionHandler_;
    
    // ZeroMQ context
    zmq::context_t context_;
    
    // Router socket for incoming connections
    std::unique_ptr<zmq::socket_t> routerSocket_;
    
    // Dealer sockets for outgoing connections
    std::unordered_map<std::string, std::unique_ptr<zmq::socket_t>> dealerSockets_;
    std::mutex socketsMutex_;
    
    // Worker threads
    std::thread routerThread_;
    std::thread pollerThread_;
    std::atomic<bool> running_{false};
    
    // Port we're listening on
    uint16_t listenPort_ = 0;
    
    Impl(PeerManager& pm) : peerManager_(pm), context_(1) {}
    
    ~Impl() {
        Stop();
    }
    
    void Start(uint16_t port) {
        if (running_) return;
        
        listenPort_ = port;
        running_ = true;
        
        try {
            // Create router socket for incoming connections
            routerSocket_ = std::make_unique<zmq::socket_t>(context_, zmq::socket_type::router);
            routerSocket_->set(zmq::sockopt::router_mandatory, 1);
            routerSocket_->set(zmq::sockopt::linger, 0);
            
            std::string bindAddr = "tcp://*:" + std::to_string(port);
            routerSocket_->bind(bindAddr);
            
            // Start worker threads
            routerThread_ = std::thread([this]() { RouteMessages(); });
            pollerThread_ = std::thread([this]() { PollMessages(); });
        } catch (const zmq::error_t& e) {
            running_ = false;
            routerSocket_.reset();
            throw;
        }
    }
    
    void Stop() {
        if (!running_) return;
        
        running_ = false;
        
        // Close all sockets
        {
            std::lock_guard<std::mutex> lock(socketsMutex_);
            dealerSockets_.clear();
        }
        
        if (routerSocket_) {
            routerSocket_->close();
            routerSocket_.reset();
        }
        
        // Wait for threads
        if (routerThread_.joinable()) {
            routerThread_.join();
        }
        if (pollerThread_.joinable()) {
            pollerThread_.join();
        }
    }
    
    void ConnectToPeer(const std::string& address, uint16_t port) {
        std::lock_guard<std::mutex> lock(socketsMutex_);
        
        try {
            // Create a dealer socket for this peer
            auto dealer = std::make_unique<zmq::socket_t>(context_, zmq::socket_type::dealer);
            
            // Set identity to our peer ID
            auto localPeer = peerManager_.GetLocalPeer();
            std::string identity = localPeer.id;
            dealer->set(zmq::sockopt::routing_id, identity);
            dealer->set(zmq::sockopt::linger, 0);
            
            // Connect to peer
            std::string connectAddr = "tcp://" + address + ":" + std::to_string(port);
            dealer->connect(connectAddr);
            
            // Store the socket
            std::string peerKey = address + ":" + std::to_string(port);
            dealerSockets_[peerKey] = std::move(dealer);
            
            // Send handshake
            auto handshake = Message::CreateHandshakeMessage(localPeer.id, localPeer.publicKey);
            SendToPeer(peerKey, handshake);
        } catch (const zmq::error_t& e) {
            std::cerr << "Failed to connect to peer " << address << ":" << port << ": " << e.what() << std::endl;
            throw;
        }
    }
    
    void DisconnectPeer(const std::string& peerId) {
        std::lock_guard<std::mutex> lock(socketsMutex_);
        
        // Find and remove socket for this peer
        auto it = dealerSockets_.find(peerId);
        if (it != dealerSockets_.end()) {
            dealerSockets_.erase(it);
            
            if (connectionHandler_) {
                connectionHandler_(peerId, false);
            }
        }
    }
    
    void SendMessage(const std::string& peerId, const Message& message) {
        SendToPeer(peerId, message);
    }
    
    void BroadcastMessage(const Message& message) {
        std::lock_guard<std::mutex> lock(socketsMutex_);
        
        // Send to all connected peers
        for (const auto& [peerId, socket] : dealerSockets_) {
            SendToPeer(peerId, message);
        }
        
        // Also send to any peers connected to our router
        auto connectedPeers = peerManager_.GetAllPeers();
        for (const auto& peer : connectedPeers) {
            if (peer.isConnected) {
                SendViaRouter(peer.id, message);
            }
        }
    }
    
    std::vector<std::string> GetConnectedPeers() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(socketsMutex_));
        std::vector<std::string> peers;
        
        // Get peers from dealer sockets
        for (const auto& [peerId, socket] : dealerSockets_) {
            peers.push_back(peerId);
        }
        
        // Also get peers connected to router
        auto allPeers = peerManager_.GetAllPeers();
        for (const auto& peer : allPeers) {
            if (peer.isConnected) {
                peers.push_back(peer.id);
            }
        }
        
        return peers;
    }
    
private:
    void SendToPeer(const std::string& peerId, const Message& message) {
        std::lock_guard<std::mutex> lock(socketsMutex_);
        
        auto it = dealerSockets_.find(peerId);
        if (it != dealerSockets_.end()) {
            try {
                auto data = message.Serialize();
                zmq::message_t msg(data.data(), data.size());
                it->second->send(msg, zmq::send_flags::dontwait);
            } catch (const zmq::error_t& e) {
                std::cerr << "Failed to send message: " << e.what() << std::endl;
            }
        }
    }
    
    void SendViaRouter(const std::string& peerId, const Message& message) {
        if (!routerSocket_) return;
        
        try {
            // Send peer ID frame
            zmq::message_t idMsg(peerId.data(), peerId.size());
            routerSocket_->send(idMsg, zmq::send_flags::sndmore);
            
            // Send message
            auto data = message.Serialize();
            zmq::message_t msg(data.data(), data.size());
            routerSocket_->send(msg, zmq::send_flags::dontwait);
        } catch (const zmq::error_t& e) {
            // Peer might not be connected via router
        }
    }
    
    void RouteMessages() {
        while (running_) {
            try {
                zmq::pollitem_t items[] = {
                    { static_cast<void*>(*routerSocket_), 0, ZMQ_POLLIN, 0 }
                };
                
                zmq::poll(items, 1, std::chrono::milliseconds(100));
                
                if (items[0].revents & ZMQ_POLLIN) {
                    // Receive identity frame
                    zmq::message_t identity;
                    auto result1 = routerSocket_->recv(identity);
                    if (!result1) continue;
                    
                    // Receive message frame
                    zmq::message_t msgFrame;
                    auto result2 = routerSocket_->recv(msgFrame);
                    if (!result2) continue;
                    
                    // Process message
                    std::string senderId(static_cast<char*>(identity.data()), identity.size());
                    std::vector<uint8_t> data(static_cast<uint8_t*>(msgFrame.data()), 
                                             static_cast<uint8_t*>(msgFrame.data()) + msgFrame.size());
                    
                    try {
                        Message msg = Message::Deserialize(data);
                        HandleMessage(senderId, msg);
                    } catch (const std::exception& e) {
                        std::cerr << "Failed to deserialize message: " << e.what() << std::endl;
                    }
                }
            } catch (const zmq::error_t& e) {
                if (running_) {
                    std::cerr << "Router error: " << e.what() << std::endl;
                }
            }
        }
    }
    
    void PollMessages() {
        while (running_) {
            std::vector<zmq::pollitem_t> items;
            std::vector<std::string> peerIds;
            
            {
                std::lock_guard<std::mutex> lock(socketsMutex_);
                for (const auto& [peerId, socket] : dealerSockets_) {
                    items.push_back({ static_cast<void*>(*socket), 0, ZMQ_POLLIN, 0 });
                    peerIds.push_back(peerId);
                }
            }
            
            if (items.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            
            try {
                zmq::poll(items.data(), items.size(), std::chrono::milliseconds(100));
                
                for (size_t i = 0; i < items.size(); ++i) {
                    if (items[i].revents & ZMQ_POLLIN) {
                        zmq::message_t msgFrame;
                        
                        std::lock_guard<std::mutex> lock(socketsMutex_);
                        auto it = dealerSockets_.find(peerIds[i]);
                        if (it != dealerSockets_.end()) {
                            auto result = it->second->recv(msgFrame);
                            if (!result) continue;
                            
                            std::vector<uint8_t> data(static_cast<uint8_t*>(msgFrame.data()), 
                                                     static_cast<uint8_t*>(msgFrame.data()) + msgFrame.size());
                            
                            try {
                                Message msg = Message::Deserialize(data);
                                HandleMessage(peerIds[i], msg);
                            } catch (const std::exception& e) {
                                std::cerr << "Failed to deserialize message: " << e.what() << std::endl;
                            }
                        }
                    }
                }
            } catch (const zmq::error_t& e) {
                if (running_) {
                    std::cerr << "Poll error: " << e.what() << std::endl;
                }
            }
        }
    }
    
    void HandleMessage(const std::string& senderId, const Message& msg) {
        if (msg.GetType() == MessageType::HANDSHAKE) {
            auto payload = msg.GetPayload();
            if (payload.size() >= 2) {
                uint16_t idLen = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];
                if (payload.size() >= static_cast<size_t>(2 + idLen)) {
                    std::string peerId(payload.begin() + 2, payload.begin() + 2 + idLen);
                    std::vector<uint8_t> publicKey(payload.begin() + 2 + idLen, payload.end());
                    
                    // Update peer info
                    PeerInfo peer;
                    peer.id = peerId;
                    peer.publicKey = publicKey;
                    peer.isConnected = true;
                    peer.lastSeen = std::chrono::system_clock::now();
                    
                    // For incoming connections via router, we don't have address/port
                    // For outgoing, parse from senderId if it's in address:port format
                    auto colonPos = senderId.find(':');
                    if (colonPos != std::string::npos) {
                        peer.address = senderId.substr(0, colonPos);
                        peer.port = std::stoi(senderId.substr(colonPos + 1));
                    }
                    
                    peerManager_.AddPeer(peer);
                    
                    if (connectionHandler_) {
                        connectionHandler_(peerId, true);
                    }
                    
                    // Send handshake response if this is incoming
                    if (senderId == peerId) {
                        auto localPeer = peerManager_.GetLocalPeer();
                        auto response = Message::CreateHandshakeMessage(localPeer.id, localPeer.publicKey);
                        SendViaRouter(peerId, response);
                    }
                }
            }
        }
        
        // Forward to user handler
        if (userMessageHandler_) {
            // Extract actual peer ID from handshake or use senderId
            std::string actualPeerId = senderId;
            if (msg.GetType() == MessageType::HANDSHAKE) {
                auto payload = msg.GetPayload();
                if (payload.size() >= 2) {
                    uint16_t idLen = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];
                    if (payload.size() >= static_cast<size_t>(2 + idLen)) {
                        actualPeerId = std::string(payload.begin() + 2, payload.begin() + 2 + idLen);
                    }
                }
            }
            userMessageHandler_(actualPeerId, msg);
        }
    }
};

NetworkManager::NetworkManager(PeerManager& peerManager)
    : pImpl_(std::make_unique<Impl>(peerManager)) {}

NetworkManager::~NetworkManager() {
    Stop();
}

void NetworkManager::Start(uint16_t port) {
    pImpl_->Start(port);
}

void NetworkManager::Stop() {
    pImpl_->Stop();
}

void NetworkManager::ConnectToPeer(const std::string& address, uint16_t port) {
    pImpl_->ConnectToPeer(address, port);
}

void NetworkManager::DisconnectPeer(const std::string& peerId) {
    pImpl_->DisconnectPeer(peerId);
}

void NetworkManager::SendMessage(const std::string& peerId, const Message& message) {
    pImpl_->SendMessage(peerId, message);
}

void NetworkManager::BroadcastMessage(const Message& message) {
    pImpl_->BroadcastMessage(message);
}

void NetworkManager::SetMessageHandler(MessageHandler handler) {
    pImpl_->userMessageHandler_ = handler;
}

void NetworkManager::SetConnectionHandler(ConnectionHandler handler) {
    pImpl_->connectionHandler_ = handler;
}

std::vector<std::string> NetworkManager::GetConnectedPeers() const {
    return pImpl_->GetConnectedPeers();
}

} // namespace p2p