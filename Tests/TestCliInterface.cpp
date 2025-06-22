#include <gtest/gtest.h>
#include "CliInterface.hpp"
#include "Network.hpp"
#include "PeerManager.hpp"
#include "Crypto.hpp"
#include <sstream>
#include <thread>

using namespace p2p;

class CLIInterfaceTest : public ::testing::Test {
protected:
    boost::asio::io_context ioContext;
    std::unique_ptr<PeerManager> peerManager;
    std::unique_ptr<NetworkManager> network;
    std::unique_ptr<CryptoManager> crypto;
    std::unique_ptr<CLIInterface> cli;
    std::thread ioThread;
    
    void SetUp() override {
        peerManager = std::make_unique<PeerManager>();
        network = std::make_unique<NetworkManager>(ioContext, *peerManager);
        crypto = std::make_unique<CryptoManager>();
        cli = std::make_unique<CLIInterface>(*network, *peerManager, *crypto);
        
        // Run io_context in separate thread
        ioThread = std::thread([this]() {
            boost::asio::io_context::work work(ioContext);
            ioContext.run();
        });
    }
    
    void TearDown() override {
        if (cli) {
            cli->Stop();
        }
        ioContext.stop();
        if (ioThread.joinable()) {
            ioThread.join();
        }
    }
};

TEST_F(CLIInterfaceTest, CreateAndDestroy) {
    // Test basic creation and destruction
    EXPECT_NE(cli, nullptr);
}

TEST_F(CLIInterfaceTest, StartStop) {
    // Test starting and stopping CLI
    std::thread cliThread([this]() {
        // Run would block, so we just test Stop
        cli->Stop();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    if (cliThread.joinable()) {
        cliThread.join();
    }
}

TEST_F(CLIInterfaceTest, MultipleStartStop) {
    // Test multiple stop calls
    cli->Stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    cli->Stop(); // Should not crash
}

TEST_F(CLIInterfaceTest, ConcurrentStop) {
    // Test concurrent stop calls
    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this]() {
            cli->Stop();
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
}