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

    KeyPair generateKeyPair();
    
    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& data, 
                                 const std::vector<uint8_t>& recipientPublicKey);
    
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& encryptedData,
                                 const std::vector<uint8_t>& privateKey);
    
    std::vector<uint8_t> sign(const std::vector<uint8_t>& data,
                             const std::vector<uint8_t>& privateKey);
    
    bool verify(const std::vector<uint8_t>& data,
                const std::vector<uint8_t>& signature,
                const std::vector<uint8_t>& publicKey);
    
    std::string generatePeerId(const std::vector<uint8_t>& publicKey);
    
    std::array<uint8_t, 32> deriveSharedSecret(const std::vector<uint8_t>& privateKey,
                                               const std::vector<uint8_t>& publicKey);

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace p2p