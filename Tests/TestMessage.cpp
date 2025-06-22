#include <gtest/gtest.h>
#include "Message.hpp"

using namespace p2p;

TEST(MessageTest, SerializeDeserialize) {
    std::vector<uint8_t> payload = {'T', 'e', 's', 't'};
    Message msg(MessageType::TEXT, payload);
    
    auto serialized = msg.Serialize();
    EXPECT_FALSE(serialized.empty());
    
    auto deserialized = Message::Deserialize(serialized);
    EXPECT_EQ(deserialized.GetType(), MessageType::TEXT);
    EXPECT_EQ(deserialized.GetPayload(), payload);
}

TEST(MessageTest, CreateTextMessage) {
    std::string text = "Hello, World!";
    auto msg = Message::CreateTextMessage(text);
    
    EXPECT_EQ(msg.GetType(), MessageType::TEXT);
    std::string recovered(msg.GetPayload().begin(), msg.GetPayload().end());
    EXPECT_EQ(recovered, text);
}

TEST(MessageTest, CreateHandshakeMessage) {
    std::string peerId = "1234567890abcdef";
    std::vector<uint8_t> publicKey = {1, 2, 3, 4, 5};
    
    auto msg = Message::CreateHandshakeMessage(peerId, publicKey);
    EXPECT_EQ(msg.GetType(), MessageType::HANDSHAKE);
    
    // Verify payload structure
    const auto& payload = msg.GetPayload();
    EXPECT_GE(payload.size(), 2 + peerId.size() + publicKey.size());
    
    uint16_t idLen = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];
    EXPECT_EQ(idLen, peerId.size());
    
    std::string recoveredId(payload.begin() + 2, payload.begin() + 2 + idLen);
    EXPECT_EQ(recoveredId, peerId);
    
    std::vector<uint8_t> recoveredKey(payload.begin() + 2 + idLen, payload.end());
    EXPECT_EQ(recoveredKey, publicKey);
}

TEST(MessageTest, CreatePeerListMessage) {
    std::vector<std::string> peers = {
        "192.168.1.1:8080",
        "192.168.1.2:8081",
        "192.168.1.3:8082"
    };
    
    auto msg = Message::CreatePeerListMessage(peers);
    EXPECT_EQ(msg.GetType(), MessageType::PEER_LIST);
    
    const auto& payload = msg.GetPayload();
    EXPECT_GE(payload.size(), 2);
    
    uint16_t peerCount = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];
    EXPECT_EQ(peerCount, peers.size());
}

TEST(MessageTest, CreatePingPongMessages) {
    auto ping = Message::CreatePingMessage();
    EXPECT_EQ(ping.GetType(), MessageType::PING);
    EXPECT_TRUE(ping.GetPayload().empty());
    
    auto pong = Message::CreatePongMessage();
    EXPECT_EQ(pong.GetType(), MessageType::PONG);
    EXPECT_TRUE(pong.GetPayload().empty());
}

TEST(MessageTest, MessageTimestamp) {
    Message msg;
    auto now = std::chrono::system_clock::now();
    auto msgTime = msg.GetTimestamp();
    
    // Message timestamp should be close to current time
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - msgTime).count();
    EXPECT_LT(std::abs(diff), 1000); // Within 1 second
}

TEST(MessageTest, DeserializeInvalidMessage) {
    // Too short message
    std::vector<uint8_t> tooShort = {1, 2, 3};
    EXPECT_THROW(Message::Deserialize(tooShort), std::runtime_error);
    
    // Invalid payload size - larger than actual data
    std::vector<uint8_t> invalidSize(13);
    invalidSize[0] = static_cast<uint8_t>(MessageType::TEXT);
    invalidSize[1] = 0x00; // Payload size: 100 bytes
    invalidSize[2] = 0x00;
    invalidSize[3] = 0x00;
    invalidSize[4] = 0x64; // 100 in decimal
    // Timestamp bytes (5-12)
    for (int i = 5; i < 13; ++i) {
        invalidSize[i] = 0;
    }
    // But we don't provide the 100 bytes of payload
    
    EXPECT_THROW(Message::Deserialize(invalidSize), std::runtime_error);
}

TEST(MessageTest, LargeMessagePayload) {
    // Test with large payload
    std::vector<uint8_t> largePayload(10000, 'X');
    Message msg(MessageType::TEXT, largePayload);
    
    auto serialized = msg.Serialize();
    auto deserialized = Message::Deserialize(serialized);
    
    EXPECT_EQ(deserialized.GetType(), MessageType::TEXT);
    EXPECT_EQ(deserialized.GetPayload(), largePayload);
}

TEST(MessageTest, EmptyPayload) {
    // Test with empty payload
    std::vector<uint8_t> emptyPayload;
    Message msg(MessageType::TEXT, emptyPayload);
    
    auto serialized = msg.Serialize();
    auto deserialized = Message::Deserialize(serialized);
    
    EXPECT_EQ(deserialized.GetType(), MessageType::TEXT);
    EXPECT_TRUE(deserialized.GetPayload().empty());
}

TEST(MessageTest, AllMessageTypes) {
    // Test all message types
    std::vector<MessageType> types = {
        MessageType::TEXT,
        MessageType::HANDSHAKE,
        MessageType::PEER_LIST,
        MessageType::PING,
        MessageType::PONG
    };
    
    for (auto type : types) {
        Message msg(type, {1, 2, 3});
        EXPECT_EQ(msg.GetType(), type);
        
        auto serialized = msg.Serialize();
        auto deserialized = Message::Deserialize(serialized);
        EXPECT_EQ(deserialized.GetType(), type);
    }
}

TEST(MessageTest, MessageTypeOutOfRange) {
    // Test with invalid message type
    std::vector<uint8_t> data(13);
    data[0] = 99; // Invalid message type
    for (int i = 1; i < 13; ++i) {
        data[i] = 0;
    }
    
    // Should still deserialize but with unknown type
    EXPECT_NO_THROW(Message::Deserialize(data));
}

TEST(MessageTest, MaxPayloadSize) {
    // Test with maximum reasonable payload size (1MB)
    std::vector<uint8_t> maxPayload(1024 * 1024, 'M');
    Message msg(MessageType::TEXT, maxPayload);
    
    auto serialized = msg.Serialize();
    auto deserialized = Message::Deserialize(serialized);
    
    EXPECT_EQ(deserialized.GetType(), MessageType::TEXT);
    EXPECT_EQ(deserialized.GetPayload(), maxPayload);
}

TEST(MessageTest, UnicodeTextMessage) {
    // Test with Unicode characters
    std::string unicodeText = "Hello 世界 🌍 Привет";
    auto msg = Message::CreateTextMessage(unicodeText);
    
    EXPECT_EQ(msg.GetType(), MessageType::TEXT);
    std::string recovered(msg.GetPayload().begin(), msg.GetPayload().end());
    EXPECT_EQ(recovered, unicodeText);
}

TEST(MessageTest, BinaryDataInPayload) {
    // Test with binary data including null bytes
    std::vector<uint8_t> binaryData = {0x00, 0xFF, 0x7F, 0x80, 0x01, 0x00, 0xFE};
    Message msg(MessageType::TEXT, binaryData);
    
    auto serialized = msg.Serialize();
    auto deserialized = Message::Deserialize(serialized);
    
    EXPECT_EQ(deserialized.GetPayload(), binaryData);
}