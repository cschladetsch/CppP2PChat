#pragma once

#include <string>
#include <vector>
#include <memory>
#include <array>
#include <boost/asio/ssl.hpp>

namespace p2p {

class CryptoManager {
public:
    CryptoManager();
    ~CryptoManager();

    struct KeyPair {
        std::vector<uint8_t> publicKey;
        std::vector<uint8_t> privateKey;
    };

    KeyPair GenerateKeyPair();
    
    std::vector<uint8_t> Encrypt(const std::vector<uint8_t>& data, 
                                 const std::vector<uint8_t>& recipientPublicKey);
    
    std::vector<uint8_t> Decrypt(const std::vector<uint8_t>& encryptedData,
                                 const std::vector<uint8_t>& privateKey);
    
    std::vector<uint8_t> Sign(const std::vector<uint8_t>& data,
                             const std::vector<uint8_t>& privateKey);
    
    bool Verify(const std::vector<uint8_t>& data,
                const std::vector<uint8_t>& signature,
                const std::vector<uint8_t>& publicKey);
    
    std::string GeneratePeerId(const std::vector<uint8_t>& publicKey);
    
    std::array<uint8_t, 32> DeriveSharedSecret(const std::vector<uint8_t>& privateKey,
                                               const std::vector<uint8_t>& publicKey);

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace p2p