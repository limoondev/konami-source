/**
 * Encryption.cpp
 * 
 * AES-256-GCM encryption implementation using OpenSSL.
 */

#include "Encryption.hpp"
#include "../Logger.hpp"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

#include <stdexcept>
#include <cstring>
#include <memory>

namespace konami::core::auth {

// RAII wrapper for EVP_CIPHER_CTX
struct CipherContextDeleter {
    void operator()(EVP_CIPHER_CTX* ctx) const {
        if (ctx) EVP_CIPHER_CTX_free(ctx);
    }
};
using CipherContext = std::unique_ptr<EVP_CIPHER_CTX, CipherContextDeleter>;

std::string Encryption::encrypt(const std::string& plaintext, const std::string& key) {
    if (key.size() != KEY_SIZE) {
        Logger::instance().error("Invalid key size: {} (expected {})", key.size(), KEY_SIZE);
        return "";
    }
    
    try {
        // Generate random IV
        auto iv = generateIV();
        
        // Create cipher context
        CipherContext ctx(EVP_CIPHER_CTX_new());
        if (!ctx) {
            throw std::runtime_error("Failed to create cipher context");
        }
        
        // Initialize encryption
        if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr,
                              reinterpret_cast<const unsigned char*>(key.data()),
                              iv.data()) != 1) {
            throw std::runtime_error("Failed to initialize encryption");
        }
        
        // Encrypt
        std::vector<uint8_t> ciphertext(plaintext.size() + EVP_MAX_BLOCK_LENGTH);
        int len = 0;
        int ciphertextLen = 0;
        
        if (EVP_EncryptUpdate(ctx.get(), ciphertext.data(), &len,
                             reinterpret_cast<const unsigned char*>(plaintext.data()),
                             static_cast<int>(plaintext.size())) != 1) {
            throw std::runtime_error("Encryption failed");
        }
        ciphertextLen = len;
        
        // Finalize
        if (EVP_EncryptFinal_ex(ctx.get(), ciphertext.data() + len, &len) != 1) {
            throw std::runtime_error("Encryption finalization failed");
        }
        ciphertextLen += len;
        
        // Get authentication tag
        std::vector<uint8_t> tag(TAG_SIZE);
        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, TAG_SIZE, tag.data()) != 1) {
            throw std::runtime_error("Failed to get authentication tag");
        }
        
        // Combine: IV + ciphertext + tag
        std::string result;
        result.reserve(IV_SIZE + ciphertextLen + TAG_SIZE);
        result.append(reinterpret_cast<char*>(iv.data()), IV_SIZE);
        result.append(reinterpret_cast<char*>(ciphertext.data()), ciphertextLen);
        result.append(reinterpret_cast<char*>(tag.data()), TAG_SIZE);
        
        return result;
        
    } catch (const std::exception& e) {
        Logger::instance().error("Encryption error: {}", e.what());
        return "";
    }
}

std::optional<std::string> Encryption::decrypt(const std::string& ciphertext, const std::string& key) {
    if (key.size() != KEY_SIZE) {
        Logger::instance().error("Invalid key size");
        return std::nullopt;
    }
    
    if (ciphertext.size() < IV_SIZE + TAG_SIZE) {
        Logger::instance().error("Invalid ciphertext size");
        return std::nullopt;
    }
    
    try {
        // Extract IV, ciphertext, and tag
        const uint8_t* iv = reinterpret_cast<const uint8_t*>(ciphertext.data());
        const uint8_t* encrypted = iv + IV_SIZE;
        size_t encryptedLen = ciphertext.size() - IV_SIZE - TAG_SIZE;
        const uint8_t* tag = encrypted + encryptedLen;
        
        // Create cipher context
        CipherContext ctx(EVP_CIPHER_CTX_new());
        if (!ctx) {
            throw std::runtime_error("Failed to create cipher context");
        }
        
        // Initialize decryption
        if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr,
                              reinterpret_cast<const unsigned char*>(key.data()),
                              iv) != 1) {
            throw std::runtime_error("Failed to initialize decryption");
        }
        
        // Decrypt
        std::vector<uint8_t> plaintext(encryptedLen + EVP_MAX_BLOCK_LENGTH);
        int len = 0;
        int plaintextLen = 0;
        
        if (EVP_DecryptUpdate(ctx.get(), plaintext.data(), &len,
                             encrypted, static_cast<int>(encryptedLen)) != 1) {
            throw std::runtime_error("Decryption failed");
        }
        plaintextLen = len;
        
        // Set authentication tag
        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, TAG_SIZE,
                               const_cast<uint8_t*>(tag)) != 1) {
            throw std::runtime_error("Failed to set authentication tag");
        }
        
        // Finalize and verify tag
        if (EVP_DecryptFinal_ex(ctx.get(), plaintext.data() + len, &len) != 1) {
            // Authentication failed
            Logger::instance().warn("Authentication tag verification failed");
            return std::nullopt;
        }
        plaintextLen += len;
        
        return std::string(reinterpret_cast<char*>(plaintext.data()), plaintextLen);
        
    } catch (const std::exception& e) {
        Logger::instance().error("Decryption error: {}", e.what());
        return std::nullopt;
    }
}

std::string Encryption::generateKey() {
    auto bytes = randomBytes(KEY_SIZE);
    return std::string(reinterpret_cast<char*>(bytes.data()), bytes.size());
}

std::vector<uint8_t> Encryption::generateIV() {
    return randomBytes(IV_SIZE);
}

std::string Encryption::deriveKey(
    const std::string& password,
    const std::string& salt,
    int iterations
) {
    std::vector<uint8_t> key(KEY_SIZE);
    
    if (PKCS5_PBKDF2_HMAC(
            password.c_str(),
            static_cast<int>(password.size()),
            reinterpret_cast<const unsigned char*>(salt.data()),
            static_cast<int>(salt.size()),
            iterations,
            EVP_sha256(),
            KEY_SIZE,
            key.data()) != 1) {
        throw std::runtime_error("Key derivation failed");
    }
    
    return std::string(reinterpret_cast<char*>(key.data()), key.size());
}

std::string Encryption::generateSalt(size_t length) {
    auto bytes = randomBytes(length);
    return std::string(reinterpret_cast<char*>(bytes.data()), bytes.size());
}

std::string Encryption::sha256(const std::string& data) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen = 0;
    
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return "";
    
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx, data.data(), data.size()) != 1 ||
        EVP_DigestFinal_ex(ctx, hash, &hashLen) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }
    
    EVP_MD_CTX_free(ctx);
    return std::string(reinterpret_cast<char*>(hash), hashLen);
}

std::string Encryption::sha256Hex(const std::string& data) {
    return hexEncode(sha256(data));
}

std::string Encryption::base64Encode(const std::string& data) {
    BIO* bio = BIO_new(BIO_s_mem());
    BIO* b64 = BIO_new(BIO_f_base64());
    bio = BIO_push(b64, bio);
    
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, data.data(), static_cast<int>(data.size()));
    BIO_flush(bio);
    
    BUF_MEM* bufferPtr;
    BIO_get_mem_ptr(bio, &bufferPtr);
    
    std::string result(bufferPtr->data, bufferPtr->length);
    
    BIO_free_all(bio);
    
    return result;
}

std::optional<std::string> Encryption::base64Decode(const std::string& encoded) {
    BIO* bio = BIO_new_mem_buf(encoded.data(), static_cast<int>(encoded.size()));
    BIO* b64 = BIO_new(BIO_f_base64());
    bio = BIO_push(b64, bio);
    
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    
    std::vector<uint8_t> buffer(encoded.size());
    int len = BIO_read(bio, buffer.data(), static_cast<int>(buffer.size()));
    
    BIO_free_all(bio);
    
    if (len < 0) {
        return std::nullopt;
    }
    
    return std::string(reinterpret_cast<char*>(buffer.data()), len);
}

std::string Encryption::hexEncode(const std::string& data) {
    static const char hexChars[] = "0123456789abcdef";
    
    std::string result;
    result.reserve(data.size() * 2);
    
    for (unsigned char c : data) {
        result.push_back(hexChars[c >> 4]);
        result.push_back(hexChars[c & 0x0f]);
    }
    
    return result;
}

std::optional<std::string> Encryption::hexDecode(const std::string& hex) {
    if (hex.size() % 2 != 0) {
        return std::nullopt;
    }
    
    std::string result;
    result.reserve(hex.size() / 2);
    
    for (size_t i = 0; i < hex.size(); i += 2) {
        char high = hex[i];
        char low = hex[i + 1];
        
        int highVal = (high >= 'a') ? (high - 'a' + 10) :
                     (high >= 'A') ? (high - 'A' + 10) :
                     (high - '0');
        int lowVal = (low >= 'a') ? (low - 'a' + 10) :
                    (low >= 'A') ? (low - 'A' + 10) :
                    (low - '0');
        
        if (highVal < 0 || highVal > 15 || lowVal < 0 || lowVal > 15) {
            return std::nullopt;
        }
        
        result.push_back(static_cast<char>((highVal << 4) | lowVal));
    }
    
    return result;
}

std::vector<uint8_t> Encryption::randomBytes(size_t length) {
    std::vector<uint8_t> bytes(length);
    
    if (RAND_bytes(bytes.data(), static_cast<int>(length)) != 1) {
        throw std::runtime_error("Failed to generate random bytes");
    }
    
    return bytes;
}

} // namespace konami::core::auth
