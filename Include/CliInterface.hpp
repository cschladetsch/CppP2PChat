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

    void Run();
    void Stop();

    void DisplayMessage(const std::string& peerId, const std::string& message, bool incoming = true);
    void DisplaySystemMessage(const std::string& message);
    void DisplayError(const std::string& error);
    void DisplaySuccess(const std::string& message);
    void DisplayWarning(const std::string& message);

private:
    void RegisterCommands();
    void ProcessCommand(const std::string& input);
    std::vector<std::string> ParseCommand(const std::string& input);

    void HandleConnect(const std::vector<std::string>& args);
    void HandleDisconnect(const std::vector<std::string>& args);
    void HandleList(const std::vector<std::string>& args);
    void HandleSend(const std::vector<std::string>& args);
    void HandleBroadcast(const std::vector<std::string>& args);
    void HandleHelp(const std::vector<std::string>& args);
    void HandleQuit(const std::vector<std::string>& args);
    void HandleInfo(const std::vector<std::string>& args);

    struct Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace p2p