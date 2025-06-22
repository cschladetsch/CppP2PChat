#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "Network.hpp"
#include "Message.hpp"
#include "PeerManager.hpp"
#include <thread>
#include <chrono>

using namespace p2p;

class NetworkTest : public ::testing::Test {
protected:
    boost::asio::io_context ioContext;
    std::unique_ptr<PeerManager> peerManager1;
    std::unique_ptr<PeerManager> peerManager2;
    std::unique_ptr<NetworkManager> network1;
    std::unique_ptr<NetworkManager> network2;
    std::thread ioThread;
    
    void SetUp() override {
        peerManager1 = std::make_unique<PeerManager>();
        peerManager2 = std::make_unique<PeerManager>();
        network1 = std::make_unique<NetworkManager>(ioContext, *peerManager1);
        network2 = std::make_unique<NetworkManager>(ioContext, *peerManager2);
        
        // Run io_context in separate thread
        ioThread = std::thread([this]() {
            boost::asio::io_context::work work(ioContext);
            ioContext.run();
        });
    }
    
    void TearDown() override {
        ioContext.stop();
        if (ioThread.joinable()) {
            ioThread.join();
        }
    }
};

TEST_F(NetworkTest, StartAndStop) {
    // Test starting the network
    EXPECT_NO_THROW(network1->Start(9100));
    
    // Give it time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Test stopping the network
    EXPECT_NO_THROW(network1->Stop());
}

TEST_F(NetworkTest, MultipleStartStop) {
    // Start and stop multiple times
    for (int i = 0; i < 3; ++i) {
        EXPECT_NO_THROW(network1->Start(9101 + i));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        EXPECT_NO_THROW(network1->Stop());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

TEST_F(NetworkTest, SetMessageHandler) {
    bool handlerCalled = false;
    std::string receivedPeerId;
    p2p::Message receivedMessage;
    
    network1->SetMessageHandler([&](const std::string& peerId, const p2p::Message& msg) {
        handlerCalled = true;
        receivedPeerId = peerId;
        receivedMessage = msg;
    });
    
    // Start network
    network1->Start(9102);
    
    // Create a test message
    p2p::Message testMsg = p2p::Message::CreateTextMessage("Test");
    
    // Verify handler was set (we can't directly access it, but we can verify it doesn't crash)
    EXPECT_NO_THROW(network1->SetMessageHandler(nullptr));
    EXPECT_NO_THROW(network1->SetMessageHandler([](const std::string&, const p2p::Message&){}));
}

TEST_F(NetworkTest, SetConnectionHandler) {
    bool handlerCalled = false;
    std::string connectedPeerId;
    bool connectionStatus = false;
    
    network1->SetConnectionHandler([&](const std::string& peerId, bool connected) {
        handlerCalled = true;
        connectedPeerId = peerId;
        connectionStatus = connected;
    });
    
    // Verify handler was set (we can't directly access it, but we can verify it doesn't crash)
    EXPECT_NO_THROW(network1->SetConnectionHandler(nullptr));
    EXPECT_NO_THROW(network1->SetConnectionHandler([](const std::string&, bool){}));
}

TEST_F(NetworkTest, ConnectToPeer) {
    // Start both networks
    network1->Start(9103);
    network2->Start(9104);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Set up connection handler
    bool connected = false;
    network1->SetConnectionHandler([&](const std::string& peerId, bool status) {
        connected = status;
    });
    
    // Try to connect
    EXPECT_NO_THROW(network1->ConnectToPeer("localhost", 9104));
    
    // Give it time to connect
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

TEST_F(NetworkTest, SendMessage) {
    // Start network
    network1->Start(9105);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Create a test message
    p2p::Message testMsg = p2p::Message::CreateTextMessage("Hello");
    
    // Attempt to send (will fail without connected peer, but shouldn't crash)
    EXPECT_NO_THROW(network1->SendMessage("test_peer", testMsg));
}

TEST_F(NetworkTest, BroadcastMessage) {
    // Start network
    network1->Start(9106);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Create a test message
    p2p::Message testMsg = p2p::Message::CreateTextMessage("Broadcast");
    
    // Attempt to broadcast (will have no effect without peers, but shouldn't crash)
    EXPECT_NO_THROW(network1->BroadcastMessage(testMsg));
}

TEST_F(NetworkTest, DisconnectPeer) {
    // Start network
    network1->Start(9107);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Attempt to disconnect (will have no effect without connected peer, but shouldn't crash)
    EXPECT_NO_THROW(network1->DisconnectPeer("test_peer"));
}

TEST_F(NetworkTest, InvalidPort) {
    // Port 0 is actually valid (OS assigns port), so let's test port already in use
    network1->Start(9200);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Try to start another network on same port using same io_context
    EXPECT_THROW(network2->Start(9200), std::exception);
}

TEST_F(NetworkTest, PortAlreadyInUse) {
    // Start first network
    network1->Start(9108);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Try to start second network on same port
    EXPECT_THROW(network2->Start(9108), std::exception);
}

TEST_F(NetworkTest, LargeMessageHandling) {
    // Test handling of large messages
    network1->Start(9300);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Create a 10MB message
    std::vector<uint8_t> largeData(10 * 1024 * 1024, 'L');
    p2p::Message largeMsg(MessageType::TEXT, largeData);
    
    // Should handle without crashing
    EXPECT_NO_THROW(network1->BroadcastMessage(largeMsg));
}

TEST_F(NetworkTest, RapidConnectionAttempts) {
    // Test rapid connection attempts
    network1->Start(9301);
    network2->Start(9302);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Rapidly attempt connections
    for (int i = 0; i < 50; ++i) {
        EXPECT_NO_THROW(network1->ConnectToPeer("localhost", 9302));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

TEST_F(NetworkTest, SimultaneousBidirectionalConnect) {
    // Test two peers connecting to each other simultaneously
    network1->Start(9303);
    network2->Start(9304);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Both try to connect at the same time
    std::thread t1([this]() {
        network1->ConnectToPeer("localhost", 9304);
    });
    
    std::thread t2([this]() {
        network2->ConnectToPeer("localhost", 9303);
    });
    
    t1.join();
    t2.join();
    
    // Should handle gracefully without deadlock
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}