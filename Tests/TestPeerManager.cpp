#include <gtest/gtest.h>
#include "PeerManager.hpp"
#include <filesystem>
#include <thread>
#include <set>

using namespace p2p;

class PeerManagerTest : public ::testing::Test {
protected:
    PeerManager peerManager;
    
    PeerInfo createTestPeer(const std::string& id) {
        PeerInfo peer;
        peer.id = id;
        peer.address = "192.168.1." + id;
        // Use a hash of the ID for port number to handle non-numeric IDs
        std::hash<std::string> hasher;
        peer.port = 8080 + (hasher(id) % 1000);
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
        if (std::filesystem::exists("debug_peers.txt")) {
            std::filesystem::remove("debug_peers.txt");
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
    // Create peers with explicit port numbers
    PeerInfo peer1;
    peer1.id = "peer1";
    peer1.address = "192.168.1.10";
    peer1.port = 8081;
    peer1.publicKey = {1, 2, 3, 4};
    peer1.isConnected = false;
    peer1.lastSeen = std::chrono::system_clock::now();
    
    PeerInfo peer2;
    peer2.id = "peer2";
    peer2.address = "192.168.1.20";
    peer2.port = 8082;
    peer2.publicKey = {5, 6, 7, 8};
    peer2.isConnected = false;
    peer2.lastSeen = std::chrono::system_clock::now();
    
    peerManager.AddPeer(peer1);
    peerManager.AddPeer(peer2);
    
    peerManager.SavePeersToFile("test_peers.txt");
    
    // Create new peer manager and load
    PeerManager newPeerManager;
    newPeerManager.LoadPeersFromFile("test_peers.txt");
    
    auto loaded1 = newPeerManager.GetPeer("peer1");
    auto loaded2 = newPeerManager.GetPeer("peer2");
    
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

TEST_F(PeerManagerTest, ConcurrentAddPeers) {
    // Test thread-safe peer addition
    const int numThreads = 10;
    const int peersPerThread = 10;
    std::vector<std::thread> threads;
    
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, t, peersPerThread]() {
            for (int i = 0; i < peersPerThread; ++i) {
                PeerInfo peer;
                peer.id = "thread" + std::to_string(t) + "_peer" + std::to_string(i);
                peer.address = "10.0." + std::to_string(t) + "." + std::to_string(i);
                peer.port = 5000 + (t * 100) + i;
                peer.publicKey = {static_cast<uint8_t>(t), static_cast<uint8_t>(i)};
                peer.isConnected = false;
                peer.lastSeen = std::chrono::system_clock::now();
                
                peerManager.AddPeer(peer);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // Verify all peers were added
    auto allPeers = peerManager.GetAllPeers();
    EXPECT_EQ(allPeers.size(), numThreads * peersPerThread);
}

TEST_F(PeerManagerTest, ConcurrentUpdateStatus) {
    // Add some peers first
    for (int i = 0; i < 20; ++i) {
        auto peer = createTestPeer(std::to_string(i));
        peerManager.AddPeer(peer);
    }
    
    // Concurrently update their status
    std::vector<std::thread> threads;
    for (int t = 0; t < 10; ++t) {
        threads.emplace_back([this, t]() {
            for (int i = 0; i < 20; ++i) {
                peerManager.UpdatePeerStatus(std::to_string(i), t % 2 == 0);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // Verify peers still exist
    for (int i = 0; i < 20; ++i) {
        auto peer = peerManager.GetPeer(std::to_string(i));
        EXPECT_TRUE(peer.has_value());
    }
}

TEST_F(PeerManagerTest, PeerExpiration) {
    // Test that old peers can be identified
    auto oldPeer = createTestPeer("old");
    auto recentPeer = createTestPeer("recent");
    
    // Set old peer's last seen to 1 hour ago
    oldPeer.lastSeen = std::chrono::system_clock::now() - std::chrono::hours(1);
    
    peerManager.AddPeer(oldPeer);
    peerManager.AddPeer(recentPeer);
    
    auto allPeers = peerManager.GetAllPeers();
    EXPECT_EQ(allPeers.size(), 2);
    
    // Check timestamps
    for (const auto& peer : allPeers) {
        if (peer.id == "old") {
            auto age = std::chrono::system_clock::now() - peer.lastSeen;
            EXPECT_GT(age, std::chrono::minutes(50));
        }
    }
}