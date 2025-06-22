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

    void AddPeer(const PeerInfo& peer);
    void RemovePeer(const std::string& peerId);
    void UpdatePeerStatus(const std::string& peerId, bool connected);
    
    std::optional<PeerInfo> GetPeer(const std::string& peerId) const;
    std::vector<PeerInfo> GetAllPeers() const;
    std::vector<PeerInfo> GetConnectedPeers() const;
    
    void SetLocalPeer(const PeerInfo& localPeer);
    const PeerInfo& GetLocalPeer() const;

    void SavePeersToFile(const std::string& filename);
    void LoadPeersFromFile(const std::string& filename);

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, PeerInfo> peers_;
    PeerInfo localPeer_;
};

} // namespace p2p