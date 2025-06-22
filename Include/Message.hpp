#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <variant>
#include <cstdint>

namespace p2p {

enum class MessageType : uint8_t {
    TEXT = 0,
    HANDSHAKE = 1,
    PEER_LIST = 2,
    PING = 3,
    PONG = 4,
    FILE_CHUNK = 5,
    KEY_EXCHANGE = 6
};

class Message {
public:
    Message() = default;
    Message(MessageType type, const std::vector<uint8_t>& payload);

    MessageType GetType() const { return type_; }
    const std::vector<uint8_t>& GetPayload() const { return payload_; }
    std::chrono::system_clock::time_point GetTimestamp() const { return timestamp_; }
    
    void SetType(MessageType type) { type_ = type; }
    void SetPayload(const std::vector<uint8_t>& payload) { payload_ = payload; }
    
    std::vector<uint8_t> Serialize() const;
    static Message Deserialize(const std::vector<uint8_t>& data);

    static Message CreateTextMessage(const std::string& text);
    static Message CreateHandshakeMessage(const std::string& peerId, 
                                        const std::vector<uint8_t>& publicKey);
    static Message CreatePeerListMessage(const std::vector<std::string>& peers);
    static Message CreatePingMessage();
    static Message CreatePongMessage();

private:
    MessageType type_ = MessageType::TEXT;
    std::vector<uint8_t> payload_;
    std::chrono::system_clock::time_point timestamp_ = std::chrono::system_clock::now();
};

} // namespace p2p