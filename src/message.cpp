#include "message.hpp"
#include <cstring>
#include <stdexcept>

namespace p2p {

Message::Message(MessageType type, const std::vector<uint8_t>& payload)
    : type_(type), payload_(payload) {}

std::vector<uint8_t> Message::serialize() const {
    std::vector<uint8_t> result;
    
    // Header: [Type(1) | PayloadSize(4) | Timestamp(8)]
    result.push_back(static_cast<uint8_t>(type_));
    
    uint32_t payloadSize = static_cast<uint32_t>(payload_.size());
    result.push_back((payloadSize >> 24) & 0xFF);
    result.push_back((payloadSize >> 16) & 0xFF);
    result.push_back((payloadSize >> 8) & 0xFF);
    result.push_back(payloadSize & 0xFF);
    
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp_.time_since_epoch()).count();
    for (int i = 7; i >= 0; --i) {
        result.push_back((timestamp >> (i * 8)) & 0xFF);
    }
    
    result.insert(result.end(), payload_.begin(), payload_.end());
    
    return result;
}

Message Message::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 13) {
        throw std::runtime_error("Invalid message: too short");
    }
    
    Message msg;
    msg.type_ = static_cast<MessageType>(data[0]);
    
    uint32_t payloadSize = (static_cast<uint32_t>(data[1]) << 24) |
                          (static_cast<uint32_t>(data[2]) << 16) |
                          (static_cast<uint32_t>(data[3]) << 8) |
                          static_cast<uint32_t>(data[4]);
    
    if (data.size() < 13 + payloadSize) {
        throw std::runtime_error("Invalid message: payload size mismatch");
    }
    
    int64_t timestamp = 0;
    for (int i = 0; i < 8; ++i) {
        timestamp = (timestamp << 8) | data[5 + i];
    }
    msg.timestamp_ = std::chrono::system_clock::time_point(
        std::chrono::milliseconds(timestamp));
    
    msg.payload_.assign(data.begin() + 13, data.begin() + 13 + payloadSize);
    
    return msg;
}

Message Message::createTextMessage(const std::string& text) {
    std::vector<uint8_t> payload(text.begin(), text.end());
    return Message(MessageType::TEXT, payload);
}

Message Message::createHandshakeMessage(const std::string& peerId,
                                      const std::vector<uint8_t>& publicKey) {
    std::vector<uint8_t> payload;
    
    // PeerId length (2 bytes) + PeerId + PublicKey
    uint16_t idLen = static_cast<uint16_t>(peerId.size());
    payload.push_back((idLen >> 8) & 0xFF);
    payload.push_back(idLen & 0xFF);
    
    payload.insert(payload.end(), peerId.begin(), peerId.end());
    payload.insert(payload.end(), publicKey.begin(), publicKey.end());
    
    return Message(MessageType::HANDSHAKE, payload);
}

Message Message::createPeerListMessage(const std::vector<std::string>& peers) {
    std::vector<uint8_t> payload;
    
    // Number of peers (2 bytes)
    uint16_t peerCount = static_cast<uint16_t>(peers.size());
    payload.push_back((peerCount >> 8) & 0xFF);
    payload.push_back(peerCount & 0xFF);
    
    // Each peer: length (2 bytes) + address string
    for (const auto& peer : peers) {
        uint16_t len = static_cast<uint16_t>(peer.size());
        payload.push_back((len >> 8) & 0xFF);
        payload.push_back(len & 0xFF);
        payload.insert(payload.end(), peer.begin(), peer.end());
    }
    
    return Message(MessageType::PEER_LIST, payload);
}

Message Message::createPingMessage() {
    return Message(MessageType::PING, {});
}

Message Message::createPongMessage() {
    return Message(MessageType::PONG, {});
}

} // namespace p2p