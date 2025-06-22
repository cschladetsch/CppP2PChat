#include "peer_manager.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <optional>

namespace p2p {

PeerManager::PeerManager() = default;
PeerManager::~PeerManager() = default;

void PeerManager::addPeer(const PeerInfo& peer) {
    std::lock_guard<std::mutex> lock(mutex_);
    peers_[peer.id] = peer;
}

void PeerManager::removePeer(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    peers_.erase(peerId);
}

void PeerManager::updatePeerStatus(const std::string& peerId, bool connected) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = peers_.find(peerId);
    if (it != peers_.end()) {
        it->second.isConnected = connected;
        it->second.lastSeen = std::chrono::system_clock::now();
    }
}

std::optional<PeerInfo> PeerManager::getPeer(const std::string& peerId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = peers_.find(peerId);
    if (it != peers_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<PeerInfo> PeerManager::getAllPeers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<PeerInfo> result;
    for (const auto& [id, peer] : peers_) {
        result.push_back(peer);
    }
    return result;
}

std::vector<PeerInfo> PeerManager::getConnectedPeers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<PeerInfo> result;
    for (const auto& [id, peer] : peers_) {
        if (peer.isConnected) {
            result.push_back(peer);
        }
    }
    return result;
}

void PeerManager::setLocalPeer(const PeerInfo& localPeer) {
    std::lock_guard<std::mutex> lock(mutex_);
    localPeer_ = localPeer;
}

const PeerInfo& PeerManager::getLocalPeer() const {
    return localPeer_;
}

void PeerManager::savePeersToFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream file(filename);
    if (!file.is_open()) return;
    
    for (const auto& [id, peer] : peers_) {
        file << peer.id << "|" 
             << peer.address << "|" 
             << peer.port << "|";
        
        // Save public key as hex
        for (uint8_t byte : peer.publicKey) {
            file << std::hex << std::setw(2) << std::setfill('0') 
                 << static_cast<int>(byte);
        }
        file << "\n";
    }
}

void PeerManager::loadPeersFromFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ifstream file(filename);
    if (!file.is_open()) return;
    
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string id, address, portStr, keyHex;
        
        if (!std::getline(iss, id, '|') ||
            !std::getline(iss, address, '|') ||
            !std::getline(iss, portStr, '|') ||
            !std::getline(iss, keyHex)) {
            continue;
        }
        
        PeerInfo peer;
        peer.id = id;
        peer.address = address;
        peer.port = static_cast<uint16_t>(std::stoul(portStr));
        peer.isConnected = false;
        peer.lastSeen = std::chrono::system_clock::now();
        
        // Parse hex public key
        for (size_t i = 0; i < keyHex.length(); i += 2) {
            std::string byteStr = keyHex.substr(i, 2);
            peer.publicKey.push_back(static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16)));
        }
        
        peers_[peer.id] = peer;
    }
}

} // namespace p2p