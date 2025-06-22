#include "Network.hpp"
#include "PeerManager.hpp"
#include "Crypto.hpp"
#include "CliInterface.hpp"
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <thread>
#include <csignal>
#include <atomic>

namespace po = boost::program_options;

std::atomic<bool> g_running{true};
boost::asio::io_context* g_ioContext = nullptr;

void signalHandler(int signum) {
    g_running = false;
    if (g_ioContext) {
        g_ioContext->stop();
    }
}

int main(int argc, char* argv[]) {
    try {
        // Parse command line options
        po::options_description desc("P2P Chat Options");
        desc.add_options()
            ("help,h", "Show help message")
            ("port,p", po::value<uint16_t>()->default_value(8080), "Local port to listen on")
            ("connect,c", po::value<std::string>(), "Connect to peer (format: address:port)")
            ("peers-file,f", po::value<std::string>()->default_value("peers.txt"), "File to save/load peers");
        
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
        
        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 0;
        }
        
        uint16_t port = vm["port"].as<uint16_t>();
        std::string peersFile = vm["peers-file"].as<std::string>();
        
        // Set up signal handling
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        
        // Initialize components
        boost::asio::io_context ioContext;
        g_ioContext = &ioContext;
        
        p2p::CryptoManager crypto;
        p2p::PeerManager peerManager;
        p2p::NetworkManager network(ioContext, peerManager);
        
        // Generate local peer identity
        auto keyPair = crypto.GenerateKeyPair();
        std::string peerId = crypto.GeneratePeerId(keyPair.publicKey);
        
        p2p::PeerInfo localPeer;
        localPeer.id = peerId;
        localPeer.address = "0.0.0.0";
        localPeer.port = port;
        localPeer.publicKey = keyPair.publicKey;
        localPeer.isConnected = true;
        peerManager.SetLocalPeer(localPeer);
        
        std::cout << "Starting P2P Chat System" << std::endl;
        std::cout << "Local peer ID: " << peerId << std::endl;
        std::cout << "Listening on port: " << port << std::endl;
        
        // Load peers from file
        peerManager.LoadPeersFromFile(peersFile);
        
        // Start network
        network.Start(port);
        
        // Start IO context in separate thread
        std::thread ioThread([&ioContext]() {
            boost::asio::io_context::work work(ioContext);
            ioContext.run();
        });
        
        // Connect to initial peer if specified
        if (vm.count("connect")) {
            std::string connectStr = vm["connect"].as<std::string>();
            size_t colonPos = connectStr.find(':');
            if (colonPos != std::string::npos) {
                std::string address = connectStr.substr(0, colonPos);
                uint16_t peerPort = static_cast<uint16_t>(
                    std::stoul(connectStr.substr(colonPos + 1)));
                network.ConnectToPeer(address, peerPort);
            }
        }
        
        // Start CLI interface
        p2p::CLIInterface cli(network, peerManager, crypto);
        
        // Run CLI in main thread
        std::thread cliThread([&cli]() {
            cli.Run();
            g_running = false;
        });
        
        // Wait for shutdown
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Cleanup
        cli.Stop();
        network.Stop();
        peerManager.SavePeersToFile(peersFile);
        
        ioContext.stop();
        if (ioThread.joinable()) {
            ioThread.join();
        }
        if (cliThread.joinable()) {
            cliThread.join();
        }
        
        std::cout << "P2P Chat System shut down successfully" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}