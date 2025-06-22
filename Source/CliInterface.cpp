#include "CliInterface.hpp"
#include "Network.hpp"
#include "PeerManager.hpp"
#include "Crypto.hpp"
#include "Message.hpp"
#include <rang.hpp>
#include <replxx.hxx>
#include <iostream>
#include <sstream>
#include <thread>
#include <atomic>
#include <iomanip>
#include <queue>

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
        // Remove vi mode keybindings for now as they may not be compatible with this version
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
    : pImpl_(std::make_unique<Impl>(network, peerManager, crypto)) {
    RegisterCommands();
}

CLIInterface::~CLIInterface() {
    Stop();
}

void CLIInterface::Run() {
    pImpl_->running = true;
    pImpl_->startDisplayThread();
    
    DisplaySystemMessage("P2P Chat System Started");
    DisplaySystemMessage("Type 'help' for available commands");
    
    // Set up message handler
    pImpl_->network.SetMessageHandler([this](const std::string& peerId, const Message& msg) {
        if (msg.GetType() == MessageType::TEXT) {
            std::string text(msg.GetPayload().begin(), msg.GetPayload().end());
            DisplayMessage(peerId, text, true);
        } else if (msg.GetType() == MessageType::HANDSHAKE) {
            // Handle handshake
            auto payload = msg.GetPayload();
            if (payload.size() >= 2) {
                uint16_t idLen = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];
                if (payload.size() >= static_cast<size_t>(2 + idLen)) {
                    std::string peerId(payload.begin() + 2, payload.begin() + 2 + idLen);
                    std::vector<uint8_t> publicKey(payload.begin() + 2 + idLen, payload.end());
                    
                    PeerInfo peer;
                    peer.id = peerId;
                    peer.publicKey = publicKey;
                    peer.isConnected = true;
                    peer.lastSeen = std::chrono::system_clock::now();
                    
                    pImpl_->peerManager.AddPeer(peer);
                    DisplaySystemMessage("Handshake received from " + peerId);
                }
            }
        }
    });
    
    // Set up connection handler
    pImpl_->network.SetConnectionHandler([this](const std::string& peerId, bool connected) {
        pImpl_->peerManager.UpdatePeerStatus(peerId, connected);
        if (connected) {
            DisplaySuccess("Connected to peer: " + peerId);
        } else {
            DisplayWarning("Disconnected from peer: " + peerId);
        }
    });
    
    // Main CLI loop
    while (pImpl_->running) {
        // Build colored prompt with lambda symbol
        std::stringstream promptStream;
        promptStream << rang::fg::magenta << "λ" << rang::style::reset 
                    << rang::style::dim << " p2p" << rang::style::reset << "> ";
        std::string prompt = promptStream.str();
        
        const char* input = pImpl_->rx.input(prompt);
        if (input == nullptr) {
            break;
        }
        
        std::string command(input);
        if (!command.empty()) {
            pImpl_->rx.history_add(input);
            ProcessCommand(command);
        }
    }
}

void CLIInterface::Stop() {
    pImpl_->running = false;
    pImpl_->displayCv.notify_all();
    if (pImpl_->displayThread.joinable()) {
        pImpl_->displayThread.join();
    }
}

void CLIInterface::DisplayMessage(const std::string& peerId, const std::string& message, bool incoming) {
    pImpl_->queueDisplay([=]() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        
        std::cout << rang::style::dim << "[" 
                  << std::put_time(&tm, "%H:%M:%S") << "] " 
                  << rang::style::reset;
        
        if (incoming) {
            std::cout << rang::fg::cyan << peerId.substr(0, 8) 
                      << rang::style::reset << ": "
                      << rang::fg::yellow << message << rang::style::reset;
        } else {
            std::cout << rang::fg::green << "You" 
                      << rang::style::reset << " -> " 
                      << rang::fg::cyan << peerId.substr(0, 8) 
                      << rang::style::reset << ": "
                      << rang::fg::yellow << message << rang::style::reset;
        }
        
        std::cout << std::endl;
    });
}

void CLIInterface::DisplaySystemMessage(const std::string& message) {
    pImpl_->queueDisplay([=]() {
        std::cout << rang::fg::blue << "[SYSTEM] " 
                  << rang::style::reset << rang::style::dim 
                  << message << rang::style::reset << std::endl;
    });
}

void CLIInterface::DisplayError(const std::string& error) {
    pImpl_->queueDisplay([=]() {
        std::cout << rang::fg::red << "[ERROR] " 
                  << rang::style::reset << rang::fg::red << rang::style::dim
                  << error << rang::style::reset << std::endl;
    });
}

void CLIInterface::DisplaySuccess(const std::string& message) {
    pImpl_->queueDisplay([=]() {
        std::cout << rang::fg::green << "[SUCCESS] " 
                  << rang::style::reset << rang::fg::green << rang::style::dim
                  << message << rang::style::reset << std::endl;
    });
}

void CLIInterface::DisplayWarning(const std::string& message) {
    pImpl_->queueDisplay([=]() {
        std::cout << rang::fg::yellow << "[WARNING] " 
                  << rang::style::reset << rang::fg::yellow << rang::style::dim
                  << message << rang::style::reset << std::endl;
    });
}

void CLIInterface::RegisterCommands() {
    pImpl_->commands["connect"] = [this](const auto& args) { HandleConnect(args); };
    pImpl_->commands["disconnect"] = [this](const auto& args) { HandleDisconnect(args); };
    pImpl_->commands["list"] = [this](const auto& args) { HandleList(args); };
    pImpl_->commands["send"] = [this](const auto& args) { HandleSend(args); };
    pImpl_->commands["broadcast"] = [this](const auto& args) { HandleBroadcast(args); };
    pImpl_->commands["help"] = [this](const auto& args) { HandleHelp(args); };
    pImpl_->commands["quit"] = [this](const auto& args) { HandleQuit(args); };
    pImpl_->commands["exit"] = [this](const auto& args) { HandleQuit(args); };
    pImpl_->commands["info"] = [this](const auto& args) { HandleInfo(args); };
}

void CLIInterface::ProcessCommand(const std::string& input) {
    auto args = ParseCommand(input);
    if (args.empty()) return;
    
    auto it = pImpl_->commands.find(args[0]);
    if (it != pImpl_->commands.end()) {
        it->second(args);
    } else {
        DisplayError("Unknown command: " + args[0]);
    }
}

std::vector<std::string> CLIInterface::ParseCommand(const std::string& input) {
    std::vector<std::string> args;
    std::istringstream iss(input);
    std::string arg;
    while (iss >> arg) {
        args.push_back(arg);
    }
    return args;
}

void CLIInterface::HandleConnect(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        DisplayError("Usage: connect <address> <port>");
        return;
    }
    
    try {
        uint16_t port = static_cast<uint16_t>(std::stoul(args[2]));
        pImpl_->network.ConnectToPeer(args[1], port);
        DisplaySystemMessage("Connecting to " + args[1] + ":" + args[2] + "...");
    } catch (const std::exception& e) {
        DisplayError("Invalid port number");
    }
}

void CLIInterface::HandleDisconnect(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        DisplayError("Usage: disconnect <peer_id>");
        return;
    }
    
    pImpl_->network.DisconnectPeer(args[1]);
    DisplaySystemMessage("Disconnected from " + args[1]);
}

void CLIInterface::HandleList(const std::vector<std::string>& args) {
    auto peers = pImpl_->peerManager.GetAllPeers();
    
    if (peers.empty()) {
        DisplaySystemMessage("No peers connected");
        return;
    }
    
    DisplaySystemMessage("Connected peers:");
    for (const auto& peer : peers) {
        std::stringstream statusStream;
        if (peer.isConnected) {
            statusStream << rang::fg::green << "[ONLINE]";
        } else {
            statusStream << rang::fg::red << "[OFFLINE]";
        }
        std::string status = statusStream.str();
        
        pImpl_->queueDisplay([=]() {
            std::cout << "  " << status << rang::style::reset 
                      << " " << peer.id << " - " 
                      << peer.address << ":" << peer.port << std::endl;
        });
    }
}

void CLIInterface::HandleSend(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        DisplayError("Usage: send <peer_id> <message>");
        return;
    }
    
    std::string message;
    for (size_t i = 2; i < args.size(); ++i) {
        if (i > 2) message += " ";
        message += args[i];
    }
    
    auto msg = Message::CreateTextMessage(message);
    pImpl_->network.SendMessage(args[1], msg);
    DisplayMessage(args[1], message, false);
}

void CLIInterface::HandleBroadcast(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        DisplayError("Usage: broadcast <message>");
        return;
    }
    
    std::string message;
    for (size_t i = 1; i < args.size(); ++i) {
        if (i > 1) message += " ";
        message += args[i];
    }
    
    auto msg = Message::CreateTextMessage(message);
    pImpl_->network.BroadcastMessage(msg);
    DisplayMessage("all", message, false);
}

void CLIInterface::HandleHelp(const std::vector<std::string>& args) {
    DisplaySystemMessage("Available commands:");
    pImpl_->queueDisplay([]() {
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

void CLIInterface::HandleQuit(const std::vector<std::string>& args) {
    DisplaySystemMessage("Shutting down...");
    pImpl_->running = false;
}

void CLIInterface::HandleInfo(const std::vector<std::string>& args) {
    auto localPeer = pImpl_->peerManager.GetLocalPeer();
    DisplaySystemMessage("Local peer information:");
    pImpl_->queueDisplay([=]() {
        std::cout << "  ID: " << localPeer.id << "\n"
                  << "  Address: " << localPeer.address << ":" << localPeer.port << "\n"
                  << "  Public Key Size: " << localPeer.publicKey.size() << " bytes\n";
    });
}

} // namespace p2p