#include "crypto.hpp"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/aes.h>
#include <openssl/sha.h>
#include <openssl/ec.h>
#include <openssl/ecdh.h>
#include <openssl/ecdsa.h>
#include <openssl/err.h>
#include <sstream>
#include <iomanip>

namespace p2p {

struct CryptoManager::Impl {
    Impl() {
        OpenSSL_add_all_algorithms();
        ERR_load_crypto_strings();
    }

    ~Impl() {
        EVP_cleanup();
        ERR_free_strings();
    }

    EVP_PKEY* generateECKeyPair() {
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
        if (!ctx) return nullptr;

        EVP_PKEY* pkey = nullptr;
        if (EVP_PKEY_keygen_init(ctx) <= 0) {
            EVP_PKEY_CTX_free(ctx);
            return nullptr;
        }

        if (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_X9_62_prime256v1) <= 0) {
            EVP_PKEY_CTX_free(ctx);
            return nullptr;
        }

        if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
            EVP_PKEY_CTX_free(ctx);
            return nullptr;
        }

        EVP_PKEY_CTX_free(ctx);
        return pkey;
    }

    std::vector<uint8_t> serializePublicKey(EVP_PKEY* pkey) {
        BIO* bio = BIO_new(BIO_s_mem());
        if (!bio) return {};

        if (PEM_write_bio_PUBKEY(bio, pkey) != 1) {
            BIO_free(bio);
            return {};
        }

        char* data = nullptr;
        long len = BIO_get_mem_data(bio, &data);
        std::vector<uint8_t> result(data, data + len);
        BIO_free(bio);
        return result;
    }

    std::vector<uint8_t> serializePrivateKey(EVP_PKEY* pkey) {
        BIO* bio = BIO_new(BIO_s_mem());
        if (!bio) return {};

        if (PEM_write_bio_PrivateKey(bio, pkey, nullptr, nullptr, 0, nullptr, nullptr) != 1) {
            BIO_free(bio);
            return {};
        }

        char* data = nullptr;
        long len = BIO_get_mem_data(bio, &data);
        std::vector<uint8_t> result(data, data + len);
        BIO_free(bio);
        return result;
    }

    EVP_PKEY* deserializePublicKey(const std::vector<uint8_t>& keyData) {
        BIO* bio = BIO_new_mem_buf(keyData.data(), keyData.size());
        if (!bio) return nullptr;

        EVP_PKEY* pkey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
        BIO_free(bio);
        return pkey;
    }

    EVP_PKEY* deserializePrivateKey(const std::vector<uint8_t>& keyData) {
        BIO* bio = BIO_new_mem_buf(keyData.data(), keyData.size());
        if (!bio) return nullptr;

        EVP_PKEY* pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
        BIO_free(bio);
        return pkey;
    }
};

CryptoManager::CryptoManager() : pImpl(std::make_unique<Impl>()) {}
CryptoManager::~CryptoManager() = default;

CryptoManager::KeyPair CryptoManager::generateKeyPair() {
    EVP_PKEY* pkey = pImpl->generateECKeyPair();
    if (!pkey) return {};

    KeyPair keyPair;
    keyPair.publicKey = pImpl->serializePublicKey(pkey);
    keyPair.privateKey = pImpl->serializePrivateKey(pkey);

    EVP_PKEY_free(pkey);
    return keyPair;
}

std::vector<uint8_t> CryptoManager::encrypt(const std::vector<uint8_t>& data,
                                           const std::vector<uint8_t>& recipientPublicKey) {
    EVP_PKEY* pubKey = pImpl->deserializePublicKey(recipientPublicKey);
    if (!pubKey) return {};

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pubKey, nullptr);
    if (!ctx) {
        EVP_PKEY_free(pubKey);
        return {};
    }

    if (EVP_PKEY_encrypt_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pubKey);
        return {};
    }

    size_t outlen;
    if (EVP_PKEY_encrypt(ctx, nullptr, &outlen, data.data(), data.size()) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pubKey);
        return {};
    }

    std::vector<uint8_t> encrypted(outlen);
    if (EVP_PKEY_encrypt(ctx, encrypted.data(), &outlen, data.data(), data.size()) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pubKey);
        return {};
    }

    encrypted.resize(outlen);
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pubKey);
    return encrypted;
}

std::vector<uint8_t> CryptoManager::decrypt(const std::vector<uint8_t>& encryptedData,
                                           const std::vector<uint8_t>& privateKey) {
    EVP_PKEY* privKey = pImpl->deserializePrivateKey(privateKey);
    if (!privKey) return {};

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(privKey, nullptr);
    if (!ctx) {
        EVP_PKEY_free(privKey);
        return {};
    }

    if (EVP_PKEY_decrypt_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(privKey);
        return {};
    }

    size_t outlen;
    if (EVP_PKEY_decrypt(ctx, nullptr, &outlen, encryptedData.data(), encryptedData.size()) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(privKey);
        return {};
    }

    std::vector<uint8_t> decrypted(outlen);
    if (EVP_PKEY_decrypt(ctx, decrypted.data(), &outlen, encryptedData.data(), encryptedData.size()) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(privKey);
        return {};
    }

    decrypted.resize(outlen);
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(privKey);
    return decrypted;
}

std::vector<uint8_t> CryptoManager::sign(const std::vector<uint8_t>& data,
                                        const std::vector<uint8_t>& privateKey) {
    EVP_PKEY* privKey = pImpl->deserializePrivateKey(privateKey);
    if (!privKey) return {};

    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        EVP_PKEY_free(privKey);
        return {};
    }

    if (EVP_DigestSignInit(mdctx, nullptr, EVP_sha256(), nullptr, privKey) <= 0) {
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(privKey);
        return {};
    }

    if (EVP_DigestSignUpdate(mdctx, data.data(), data.size()) <= 0) {
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(privKey);
        return {};
    }

    size_t siglen;
    if (EVP_DigestSignFinal(mdctx, nullptr, &siglen) <= 0) {
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(privKey);
        return {};
    }

    std::vector<uint8_t> signature(siglen);
    if (EVP_DigestSignFinal(mdctx, signature.data(), &siglen) <= 0) {
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(privKey);
        return {};
    }

    signature.resize(siglen);
    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(privKey);
    return signature;
}

bool CryptoManager::verify(const std::vector<uint8_t>& data,
                          const std::vector<uint8_t>& signature,
                          const std::vector<uint8_t>& publicKey) {
    EVP_PKEY* pubKey = pImpl->deserializePublicKey(publicKey);
    if (!pubKey) return false;

    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        EVP_PKEY_free(pubKey);
        return false;
    }

    if (EVP_DigestVerifyInit(mdctx, nullptr, EVP_sha256(), nullptr, pubKey) <= 0) {
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(pubKey);
        return false;
    }

    if (EVP_DigestVerifyUpdate(mdctx, data.data(), data.size()) <= 0) {
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(pubKey);
        return false;
    }

    int result = EVP_DigestVerifyFinal(mdctx, signature.data(), signature.size());
    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(pubKey);
    return result == 1;
}

std::string CryptoManager::generatePeerId(const std::vector<uint8_t>& publicKey) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(publicKey.data(), publicKey.size(), hash);

    std::stringstream ss;
    for (int i = 0; i < 8; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
}

std::array<uint8_t, 32> CryptoManager::deriveSharedSecret(const std::vector<uint8_t>& privateKey,
                                                         const std::vector<uint8_t>& publicKey) {
    std::array<uint8_t, 32> sharedSecret{};
    
    EVP_PKEY* privKey = pImpl->deserializePrivateKey(privateKey);
    EVP_PKEY* pubKey = pImpl->deserializePublicKey(publicKey);
    
    if (!privKey || !pubKey) {
        if (privKey) EVP_PKEY_free(privKey);
        if (pubKey) EVP_PKEY_free(pubKey);
        return sharedSecret;
    }

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(privKey, nullptr);
    if (!ctx) {
        EVP_PKEY_free(privKey);
        EVP_PKEY_free(pubKey);
        return sharedSecret;
    }

    if (EVP_PKEY_derive_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(privKey);
        EVP_PKEY_free(pubKey);
        return sharedSecret;
    }

    if (EVP_PKEY_derive_set_peer(ctx, pubKey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(privKey);
        EVP_PKEY_free(pubKey);
        return sharedSecret;
    }

    size_t secretLen = 32;
    EVP_PKEY_derive(ctx, sharedSecret.data(), &secretLen);

    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(privKey);
    EVP_PKEY_free(pubKey);
    
    return sharedSecret;
}

} // namespace p2p