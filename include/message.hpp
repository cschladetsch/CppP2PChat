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

    MessageType getType() const { return type_; }
    const std::vector<uint8_t>& getPayload() const { return payload_; }
    std::chrono::system_clock::time_point getTimestamp() const { return timestamp_; }
    
    void setType(MessageType type) { type_ = type; }
    void setPayload(const std::vector<uint8_t>& payload) { payload_ = payload; }
    
    std::vector<uint8_t> serialize() const;
    static Message deserialize(const std::vector<uint8_t>& data);

    static Message createTextMessage(const std::string& text);
    static Message createHandshakeMessage(const std::string& peerId, 
                                        const std::vector<uint8_t>& publicKey);
    static Message createPeerListMessage(const std::vector<std::string>& peers);
    static Message createPingMessage();
    static Message createPongMessage();

private:
    MessageType type_ = MessageType::TEXT;
    std::vector<uint8_t> payload_;
    std::chrono::system_clock::time_point timestamp_ = std::chrono::system_clock::now();
};

} // namespace p2p