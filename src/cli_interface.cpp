#include "cli_interface.hpp"
#include "network.hpp"
#include "peer_manager.hpp"
#include "crypto.hpp"
#include "message.hpp"
#include <rang.hpp>
#include <replxx.hxx>
#include <iostream>
#include <sstream>
#include <thread>
#include <atomic>
#include <iomanip>

namespace p2p {

struct CLIInterface::Impl {
    NetworkManager& network;
    PeerManager& peerManager;
    CryptoManager& crypto;
    replxx::Replxx rx;
    std::atomic<bool> running{false};
    std::unordered_map<std::string, CommandHandler> commands;
    std::thread displayThread;
    std::mutex displayMutex;
    std::queue<std::function<void()>> displayQueue;
    std::condition_variable displayCv;

    Impl(NetworkManager& net, PeerManager& pm, CryptoManager& c)
        : network(net), peerManager(pm), crypto(c) {
        rx.install_window_change_handler();
        
        // Configure replxx
        rx.set_max_history_size(1000);
        rx.set_word_break_characters(" \t\n!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~");
        
        // Vi mode keybindings
        rx.bind_key(Replxx::KEY::ESCAPE, [this](char32_t) {
            rx.set_state(Replxx::State::NORMAL);
            return Replxx::ACTION::CONTINUE;
        });
    }

    void startDisplayThread() {
        displayThread = std::thread([this]() {
            while (running) {
                std::unique_lock<std::mutex> lock(displayMutex);
                displayCv.wait(lock, [this] { return !displayQueue.empty() || !running; });
                
                while (!displayQueue.empty()) {
                    auto func = displayQueue.front();
                    displayQueue.pop();
                    lock.unlock();
                    func();
                    lock.lock();
                }
            }
        });
    }

    void queueDisplay(std::function<void()> func) {
        {
            std::lock_guard<std::mutex> lock(displayMutex);
            displayQueue.push(func);
        }
        displayCv.notify_one();
    }
};

CLIInterface::CLIInterface(NetworkManager& network, PeerManager& peerManager, CryptoManager& crypto)
    : pImpl(std::make_unique<Impl>(network, peerManager, crypto)) {
    registerCommands();
}

CLIInterface::~CLIInterface() {
    stop();
}

void CLIInterface::run() {
    pImpl->running = true;
    pImpl->startDisplayThread();
    
    displaySystemMessage("P2P Chat System Started");
    displaySystemMessage("Type 'help' for available commands");
    
    // Set up message handler
    pImpl->network.setMessageHandler([this](const std::string& peerId, const Message& msg) {
        if (msg.getType() == MessageType::TEXT) {
            std::string text(msg.getPayload().begin(), msg.getPayload().end());
            displayMessage(peerId, text, true);
        } else if (msg.getType() == MessageType::HANDSHAKE) {
            // Handle handshake
            auto payload = msg.getPayload();
            if (payload.size() >= 2) {
                uint16_t idLen = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];
                if (payload.size() >= 2 + idLen) {
                    std::string peerId(payload.begin() + 2, payload.begin() + 2 + idLen);
                    std::vector<uint8_t> publicKey(payload.begin() + 2 + idLen, payload.end());
                    
                    PeerInfo peer;
                    peer.id = peerId;
                    peer.publicKey = publicKey;
                    peer.isConnected = true;
                    peer.lastSeen = std::chrono::system_clock::now();
                    
                    pImpl->peerManager.addPeer(peer);
                    displaySystemMessage("Handshake received from " + peerId);
                }
            }
        }
    });
    
    // Set up connection handler
    pImpl->network.setConnectionHandler([this](const std::string& peerId, bool connected) {
        pImpl->peerManager.updatePeerStatus(peerId, connected);
        if (connected) {
            displaySuccess("Connected to peer: " + peerId);
        } else {
            displayWarning("Disconnected from peer: " + peerId);
        }
    });
    
    // Main CLI loop
    while (pImpl->running) {
        const char* input = pImpl->rx.input("p2p> ");
        if (input == nullptr) {
            break;
        }
        
        std::string command(input);
        if (!command.empty()) {
            pImpl->rx.history_add(input);
            processCommand(command);
        }
    }
}

void CLIInterface::stop() {
    pImpl->running = false;
    pImpl->displayCv.notify_all();
    if (pImpl->displayThread.joinable()) {
        pImpl->displayThread.join();
    }
}

void CLIInterface::displayMessage(const std::string& peerId, const std::string& message, bool incoming) {
    pImpl->queueDisplay([=]() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        
        std::cout << rang::style::dim << "[" 
                  << std::put_time(&tm, "%H:%M:%S") << "] " 
                  << rang::style::reset;
        
        if (incoming) {
            std::cout << rang::fg::cyan << peerId.substr(0, 8) 
                      << rang::style::reset << ": ";
        } else {
            std::cout << rang::fg::green << "You" 
                      << rang::style::reset << " -> " 
                      << rang::fg::cyan << peerId.substr(0, 8) 
                      << rang::style::reset << ": ";
        }
        
        std::cout << message << std::endl;
    });
}

void CLIInterface::displaySystemMessage(const std::string& message) {
    pImpl->queueDisplay([=]() {
        std::cout << rang::fg::blue << "[SYSTEM] " 
                  << rang::style::reset << message << std::endl;
    });
}

void CLIInterface::displayError(const std::string& error) {
    pImpl->queueDisplay([=]() {
        std::cout << rang::fg::red << "[ERROR] " 
                  << rang::style::reset << error << std::endl;
    });
}

void CLIInterface::displaySuccess(const std::string& message) {
    pImpl->queueDisplay([=]() {
        std::cout << rang::fg::green << "[SUCCESS] " 
                  << rang::style::reset << message << std::endl;
    });
}

void CLIInterface::displayWarning(const std::string& message) {
    pImpl->queueDisplay([=]() {
        std::cout << rang::fg::yellow << "[WARNING] " 
                  << rang::style::reset << message << std::endl;
    });
}

void CLIInterface::registerCommands() {
    pImpl->commands["connect"] = [this](const auto& args) { handleConnect(args); };
    pImpl->commands["disconnect"] = [this](const auto& args) { handleDisconnect(args); };
    pImpl->commands["list"] = [this](const auto& args) { handleList(args); };
    pImpl->commands["send"] = [this](const auto& args) { handleSend(args); };
    pImpl->commands["broadcast"] = [this](const auto& args) { handleBroadcast(args); };
    pImpl->commands["help"] = [this](const auto& args) { handleHelp(args); };
    pImpl->commands["quit"] = [this](const auto& args) { handleQuit(args); };
    pImpl->commands["exit"] = [this](const auto& args) { handleQuit(args); };
    pImpl->commands["info"] = [this](const auto& args) { handleInfo(args); };
}

void CLIInterface::processCommand(const std::string& input) {
    auto args = parseCommand(input);
    if (args.empty()) return;
    
    auto it = pImpl->commands.find(args[0]);
    if (it != pImpl->commands.end()) {
        it->second(args);
    } else {
        displayError("Unknown command: " + args[0]);
    }
}

std::vector<std::string> CLIInterface::parseCommand(const std::string& input) {
    std::vector<std::string> args;
    std::istringstream iss(input);
    std::string arg;
    while (iss >> arg) {
        args.push_back(arg);
    }
    return args;
}

void CLIInterface::handleConnect(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        displayError("Usage: connect <address> <port>");
        return;
    }
    
    try {
        uint16_t port = static_cast<uint16_t>(std::stoul(args[2]));
        pImpl->network.connectToPeer(args[1], port);
        displaySystemMessage("Connecting to " + args[1] + ":" + args[2] + "...");
    } catch (const std::exception& e) {
        displayError("Invalid port number");
    }
}

void CLIInterface::handleDisconnect(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        displayError("Usage: disconnect <peer_id>");
        return;
    }
    
    pImpl->network.disconnectPeer(args[1]);
    displaySystemMessage("Disconnected from " + args[1]);
}

void CLIInterface::handleList(const std::vector<std::string>& args) {
    auto peers = pImpl->peerManager.getAllPeers();
    
    if (peers.empty()) {
        displaySystemMessage("No peers connected");
        return;
    }
    
    displaySystemMessage("Connected peers:");
    for (const auto& peer : peers) {
        std::string status = peer.isConnected ? 
            rang::fg::green + "[ONLINE]" : 
            rang::fg::red + "[OFFLINE]";
        
        pImpl->queueDisplay([=]() {
            std::cout << "  " << status << rang::style::reset 
                      << " " << peer.id << " - " 
                      << peer.address << ":" << peer.port << std::endl;
        });
    }
}

void CLIInterface::handleSend(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        displayError("Usage: send <peer_id> <message>");
        return;
    }
    
    std::string message;
    for (size_t i = 2; i < args.size(); ++i) {
        if (i > 2) message += " ";
        message += args[i];
    }
    
    auto msg = Message::createTextMessage(message);
    pImpl->network.sendMessage(args[1], msg);
    displayMessage(args[1], message, false);
}

void CLIInterface::handleBroadcast(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        displayError("Usage: broadcast <message>");
        return;
    }
    
    std::string message;
    for (size_t i = 1; i < args.size(); ++i) {
        if (i > 1) message += " ";
        message += args[i];
    }
    
    auto msg = Message::createTextMessage(message);
    pImpl->network.broadcastMessage(msg);
    displayMessage("all", message, false);
}

void CLIInterface::handleHelp(const std::vector<std::string>& args) {
    displaySystemMessage("Available commands:");
    pImpl->queueDisplay([]() {
        std::cout << "  connect <address> <port> - Connect to a peer\n"
                  << "  disconnect <peer_id>     - Disconnect from a peer\n"
                  << "  list                     - List all peers\n"
                  << "  send <peer_id> <message> - Send message to a peer\n"
                  << "  broadcast <message>      - Send message to all peers\n"
                  << "  info                     - Show local peer information\n"
                  << "  help                     - Show this help\n"
                  << "  quit/exit                - Exit the program\n";
    });
}

void CLIInterface::handleQuit(const std::vector<std::string>& args) {
    displaySystemMessage("Shutting down...");
    pImpl->running = false;
}

void CLIInterface::handleInfo(const std::vector<std::string>& args) {
    auto localPeer = pImpl->peerManager.getLocalPeer();
    displaySystemMessage("Local peer information:");
    pImpl->queueDisplay([=]() {
        std::cout << "  ID: " << localPeer.id << "\n"
                  << "  Address: " << localPeer.address << ":" << localPeer.port << "\n"
                  << "  Public Key Size: " << localPeer.publicKey.size() << " bytes\n";
    });
}

} // namespace p2p