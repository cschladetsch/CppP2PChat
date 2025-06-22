#include <gtest/gtest.h>
#include "Crypto.hpp"

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