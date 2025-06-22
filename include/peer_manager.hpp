#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace p2p {

struct PeerInfo {
    std::string id;
    std::string address;
    uint16_t port;
    std::vector<uint8_t> publicKey;
    bool isConnected;
    std::chrono::system_clock::time_point lastSeen;
};

class PeerManager {
public:
    PeerManager();
    ~PeerManager();

    void addPeer(const PeerInfo& peer);
    void removePeer(const std::string& peerId);
    void updatePeerStatus(const std::string& peerId, bool connected);
    
    std::optional<PeerInfo> getPeer(const std::string& peerId) const;
    std::vector<PeerInfo> getAllPeers() const;
    std::vector<PeerInfo> getConnectedPeers() const;
    
    void setLocalPeer(const PeerInfo& localPeer);
    const PeerInfo& getLocalPeer() const;

    void savePeersToFile(const std::string& filename);
    void loadPeersFromFile(const std::string& filename);

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, PeerInfo> peers_;
    PeerInfo localPeer_;
};

} // namespace p2p