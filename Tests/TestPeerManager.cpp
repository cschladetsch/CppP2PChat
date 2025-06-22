#include <gtest/gtest.h>
#include "PeerManager.hpp"
#include <filesystem>

using namespace p2p;

class PeerManagerTest : public ::testing::Test {
protected:
    PeerManager peerManager;
    
    PeerInfo createTestPeer(const std::string& id) {
        PeerInfo peer;
        peer.id = id;
        peer.address = "192.168.1." + id;
        peer.port = 8080 + std::stoi(id);
        peer.publicKey = {1, 2, 3};
        peer.isConnected = false;
        peer.lastSeen = std::chrono::system_clock::now();
        return peer;
    }
    
    void TearDown() override {
        // Clean up test files
        if (std::filesystem::exists("test_peers.txt")) {
            std::filesystem::remove("test_peers.txt");
        }
    }
};

TEST_F(PeerManagerTest, AddAndGetPeer) {
    auto peer = createTestPeer("1");
    peerManager.AddPeer(peer);
    
    auto retrieved = peerManager.GetPeer("1");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->id, peer.id);
    EXPECT_EQ(retrieved->address, peer.address);
    EXPECT_EQ(retrieved->port, peer.port);
}

TEST_F(PeerManagerTest, RemovePeer) {
    auto peer = createTestPeer("1");
    peerManager.AddPeer(peer);
    
    peerManager.RemovePeer("1");
    auto retrieved = peerManager.GetPeer("1");
    EXPECT_FALSE(retrieved.has_value());
}

TEST_F(PeerManagerTest, UpdatePeerStatus) {
    auto peer = createTestPeer("1");
    peerManager.AddPeer(peer);
    
    peerManager.UpdatePeerStatus("1", true);
    auto retrieved = peerManager.GetPeer("1");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_TRUE(retrieved->isConnected);
    
    peerManager.UpdatePeerStatus("1", false);
    retrieved = peerManager.GetPeer("1");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_FALSE(retrieved->isConnected);
}

TEST_F(PeerManagerTest, GetAllPeers) {
    auto peer1 = createTestPeer("1");
    auto peer2 = createTestPeer("2");
    auto peer3 = createTestPeer("3");
    
    peerManager.AddPeer(peer1);
    peerManager.AddPeer(peer2);
    peerManager.AddPeer(peer3);
    
    auto allPeers = peerManager.GetAllPeers();
    EXPECT_EQ(allPeers.size(), 3);
}

TEST_F(PeerManagerTest, GetConnectedPeers) {
    auto peer1 = createTestPeer("1");
    auto peer2 = createTestPeer("2");
    auto peer3 = createTestPeer("3");
    
    peer1.isConnected = true;
    peer3.isConnected = true;
    
    peerManager.AddPeer(peer1);
    peerManager.AddPeer(peer2);
    peerManager.AddPeer(peer3);
    
    auto connectedPeers = peerManager.GetConnectedPeers();
    EXPECT_EQ(connectedPeers.size(), 2);
}

TEST_F(PeerManagerTest, LocalPeer) {
    auto localPeer = createTestPeer("local");
    peerManager.SetLocalPeer(localPeer);
    
    const auto& retrieved = peerManager.GetLocalPeer();
    EXPECT_EQ(retrieved.id, localPeer.id);
    EXPECT_EQ(retrieved.address, localPeer.address);
    EXPECT_EQ(retrieved.port, localPeer.port);
}

TEST_F(PeerManagerTest, SaveAndLoadPeers) {
    auto peer1 = createTestPeer("1");
    auto peer2 = createTestPeer("2");
    
    peerManager.AddPeer(peer1);
    peerManager.AddPeer(peer2);
    
    peerManager.SavePeersToFile("test_peers.txt");
    
    // Create new peer manager and load
    PeerManager newPeerManager;
    newPeerManager.LoadPeersFromFile("test_peers.txt");
    
    auto loaded1 = newPeerManager.GetPeer("1");
    auto loaded2 = newPeerManager.GetPeer("2");
    
    ASSERT_TRUE(loaded1.has_value());
    ASSERT_TRUE(loaded2.has_value());
    
    EXPECT_EQ(loaded1->id, peer1.id);
    EXPECT_EQ(loaded1->address, peer1.address);
    EXPECT_EQ(loaded1->port, peer1.port);
    EXPECT_EQ(loaded1->publicKey, peer1.publicKey);
    
    EXPECT_EQ(loaded2->id, peer2.id);
    EXPECT_EQ(loaded2->address, peer2.address);
    EXPECT_EQ(loaded2->port, peer2.port);
    EXPECT_EQ(loaded2->publicKey, peer2.publicKey);
}

TEST_F(PeerManagerTest, NonExistentPeer) {
    auto retrieved = peerManager.GetPeer("nonexistent");
    EXPECT_FALSE(retrieved.has_value());
}

TEST_F(PeerManagerTest, UpdateNonExistentPeer) {
    // Should not crash when updating non-existent peer
    peerManager.UpdatePeerStatus("nonexistent", true);
    auto retrieved = peerManager.GetPeer("nonexistent");
    EXPECT_FALSE(retrieved.has_value());
}