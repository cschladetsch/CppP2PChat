#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>

namespace p2p {

class NetworkManager;
class PeerManager;
class CryptoManager;

class CLIInterface {
public:
    using CommandHandler = std::function<void(const std::vector<std::string>&)>;

    CLIInterface(NetworkManager& network, PeerManager& peerManager, CryptoManager& crypto);
    ~CLIInterface();

    void run();
    void stop();

    void displayMessage(const std::string& peerId, const std::string& message, bool incoming = true);
    void displaySystemMessage(const std::string& message);
    void displayError(const std::string& error);
    void displaySuccess(const std::string& message);
    void displayWarning(const std::string& message);

private:
    void registerCommands();
    void processCommand(const std::string& input);
    std::vector<std::string> parseCommand(const std::string& input);

    void handleConnect(const std::vector<std::string>& args);
    void handleDisconnect(const std::vector<std::string>& args);
    void handleList(const std::vector<std::string>& args);
    void handleSend(const std::vector<std::string>& args);
    void handleBroadcast(const std::vector<std::string>& args);
    void handleHelp(const std::vector<std::string>& args);
    void handleQuit(const std::vector<std::string>& args);
    void handleInfo(const std::vector<std::string>& args);

    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace p2p