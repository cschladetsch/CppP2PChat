#include <gtest/gtest.h>
#include "Crypto.hpp"
#include <set>

using namespace p2p;

class CryptoTest : public ::testing::Test {
protected:
    CryptoManager crypto;
};

TEST_F(CryptoTest, GenerateKeyPair) {
    auto keyPair = crypto.GenerateKeyPair();
    
    EXPECT_FALSE(keyPair.publicKey.empty());
    EXPECT_FALSE(keyPair.privateKey.empty());
    EXPECT_NE(keyPair.publicKey, keyPair.privateKey);
}

TEST_F(CryptoTest, GeneratePeerId) {
    auto keyPair = crypto.GenerateKeyPair();
    std::string peerId = crypto.GeneratePeerId(keyPair.publicKey);
    
    EXPECT_FALSE(peerId.empty());
    EXPECT_EQ(peerId.length(), 16); // 8 bytes in hex = 16 characters
    
    // Same key should generate same ID
    std::string peerId2 = crypto.GeneratePeerId(keyPair.publicKey);
    EXPECT_EQ(peerId, peerId2);
    
    // Different key should generate different ID
    auto keyPair2 = crypto.GenerateKeyPair();
    std::string peerId3 = crypto.GeneratePeerId(keyPair2.publicKey);
    EXPECT_NE(peerId, peerId3);
}

TEST_F(CryptoTest, SignAndVerify) {
    auto keyPair = crypto.GenerateKeyPair();
    std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o'};
    
    auto signature = crypto.Sign(data, keyPair.privateKey);
    EXPECT_FALSE(signature.empty());
    
    // Verify with correct public key
    bool valid = crypto.Verify(data, signature, keyPair.publicKey);
    EXPECT_TRUE(valid);
    
    // Verify with wrong data
    std::vector<uint8_t> wrongData = {'W', 'o', 'r', 'l', 'd'};
    valid = crypto.Verify(wrongData, signature, keyPair.publicKey);
    EXPECT_FALSE(valid);
    
    // Verify with wrong public key
    auto keyPair2 = crypto.GenerateKeyPair();
    valid = crypto.Verify(data, signature, keyPair2.publicKey);
    EXPECT_FALSE(valid);
}

TEST_F(CryptoTest, DeriveSharedSecret) {
    auto keyPair1 = crypto.GenerateKeyPair();
    auto keyPair2 = crypto.GenerateKeyPair();
    
    // Derive shared secret from both sides
    auto secret1 = crypto.DeriveSharedSecret(keyPair1.privateKey, keyPair2.publicKey);
    auto secret2 = crypto.DeriveSharedSecret(keyPair2.privateKey, keyPair1.publicKey);
    
    // Both sides should derive the same secret
    EXPECT_EQ(secret1, secret2);
    
    // Secret should be 32 bytes
    EXPECT_EQ(secret1.size(), 32);
    
    // Different key pairs should produce different secrets
    auto keyPair3 = crypto.GenerateKeyPair();
    auto secret3 = crypto.DeriveSharedSecret(keyPair1.privateKey, keyPair3.publicKey);
    EXPECT_NE(secret1, secret3);
}

TEST_F(CryptoTest, EmptyDataSign) {
    auto keyPair = crypto.GenerateKeyPair();
    std::vector<uint8_t> emptyData;
    
    // Should be able to sign empty data
    auto signature = crypto.Sign(emptyData, keyPair.privateKey);
    EXPECT_FALSE(signature.empty());
    
    // Should verify correctly
    bool valid = crypto.Verify(emptyData, signature, keyPair.publicKey);
    EXPECT_TRUE(valid);
}

TEST_F(CryptoTest, LargeDataSign) {
    auto keyPair = crypto.GenerateKeyPair();
    std::vector<uint8_t> largeData(100000, 'X');
    
    // Should be able to sign large data
    auto signature = crypto.Sign(largeData, keyPair.privateKey);
    EXPECT_FALSE(signature.empty());
    
    // Should verify correctly
    bool valid = crypto.Verify(largeData, signature, keyPair.publicKey);
    EXPECT_TRUE(valid);
}

TEST_F(CryptoTest, InvalidSignature) {
    auto keyPair = crypto.GenerateKeyPair();
    std::vector<uint8_t> data = {'T', 'e', 's', 't'};
    
    auto signature = crypto.Sign(data, keyPair.privateKey);
    
    // Corrupt the signature
    if (!signature.empty()) {
        signature[0] ^= 0xFF;
    }
    
    // Should fail verification
    bool valid = crypto.Verify(data, signature, keyPair.publicKey);
    EXPECT_FALSE(valid);
}

TEST_F(CryptoTest, MultipleKeyPairs) {
    // Generate multiple key pairs and ensure they're all different
    std::vector<CryptoManager::KeyPair> keyPairs;
    for (int i = 0; i < 10; ++i) {
        keyPairs.push_back(crypto.GenerateKeyPair());
    }
    
    // Check all pairs are unique
    for (size_t i = 0; i < keyPairs.size(); ++i) {
        for (size_t j = i + 1; j < keyPairs.size(); ++j) {
            EXPECT_NE(keyPairs[i].publicKey, keyPairs[j].publicKey);
            EXPECT_NE(keyPairs[i].privateKey, keyPairs[j].privateKey);
        }
    }
}

TEST_F(CryptoTest, KeyPairSizes) {
    auto keyPair = crypto.GenerateKeyPair();
    
    // Verify key sizes are reasonable
    EXPECT_GT(keyPair.publicKey.size(), 32);  // At least 256 bits
    EXPECT_GT(keyPair.privateKey.size(), 32);
    EXPECT_LT(keyPair.publicKey.size(), 1024); // Not unreasonably large
    EXPECT_LT(keyPair.privateKey.size(), 1024);
}

TEST_F(CryptoTest, DeterministicPeerId) {
    // Same public key should always generate same peer ID
    auto keyPair = crypto.GenerateKeyPair();
    
    std::set<std::string> peerIds;
    for (int i = 0; i < 100; ++i) {
        peerIds.insert(crypto.GeneratePeerId(keyPair.publicKey));
    }
    
    // All IDs should be identical
    EXPECT_EQ(peerIds.size(), 1);
}

TEST_F(CryptoTest, SignatureConsistency) {
    auto keyPair = crypto.GenerateKeyPair();
    std::vector<uint8_t> data = {'C', 'o', 'n', 's', 'i', 's', 't', 'e', 'n', 't'};
    
    // Generate multiple signatures for same data
    std::vector<std::vector<uint8_t>> signatures;
    for (int i = 0; i < 5; ++i) {
        signatures.push_back(crypto.Sign(data, keyPair.privateKey));
    }
    
    // All signatures should verify correctly
    for (const auto& sig : signatures) {
        EXPECT_TRUE(crypto.Verify(data, sig, keyPair.publicKey));
    }
}

TEST_F(CryptoTest, CrossKeyVerification) {
    // Ensure signatures can't be verified with wrong keys
    auto keyPair1 = crypto.GenerateKeyPair();
    auto keyPair2 = crypto.GenerateKeyPair();
    auto keyPair3 = crypto.GenerateKeyPair();
    
    std::vector<uint8_t> data = {'T', 'e', 's', 't'};
    auto sig1 = crypto.Sign(data, keyPair1.privateKey);
    
    // Should only verify with keyPair1's public key
    EXPECT_TRUE(crypto.Verify(data, sig1, keyPair1.publicKey));
    EXPECT_FALSE(crypto.Verify(data, sig1, keyPair2.publicKey));
    EXPECT_FALSE(crypto.Verify(data, sig1, keyPair3.publicKey));
}