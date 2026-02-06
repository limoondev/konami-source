#pragma once

/**
 * Encryption.hpp
 * 
 * AES-256-GCM encryption utilities using OpenSSL.
 */

#include <string>
#include <optional>
#include <vector>
#include <cstdint>

namespace nexus::core::auth {

/**
 * Encryption - AES-256-GCM encryption utilities
 */
class Encryption {
public:
    // Constants
    static constexpr size_t KEY_SIZE = 32;      // 256 bits
    static constexpr size_t IV_SIZE = 12;       // 96 bits (recommended for GCM)
    static constexpr size_t TAG_SIZE = 16;      // 128 bits

    /**
     * Encrypt data using AES-256-GCM
     * @param plaintext Data to encrypt
     * @param key Encryption key (32 bytes)
     * @return Encrypted data (IV + ciphertext + tag) or empty on error
     */
    static std::string encrypt(const std::string& plaintext, const std::string& key);
    
    /**
     * Decrypt data using AES-256-GCM
     * @param ciphertext Encrypted data (IV + ciphertext + tag)
     * @param key Encryption key (32 bytes)
     * @return Decrypted data or nullopt on error
     */
    static std::optional<std::string> decrypt(const std::string& ciphertext, const std::string& key);
    
    /**
     * Generate a random encryption key
     * @return 32-byte random key
     */
    static std::string generateKey();
    
    /**
     * Generate a random IV
     * @return 12-byte random IV
     */
    static std::vector<uint8_t> generateIV();
    
    /**
     * Derive key from password using PBKDF2
     * @param password Password to derive from
     * @param salt Salt value
     * @param iterations PBKDF2 iterations (default: 100000)
     * @return Derived key
     */
    static std::string deriveKey(
        const std::string& password,
        const std::string& salt,
        int iterations = 100000
    );
    
    /**
     * Generate random salt for PBKDF2
     * @param length Salt length in bytes (default: 16)
     * @return Random salt
     */
    static std::string generateSalt(size_t length = 16);
    
    /**
     * Compute SHA-256 hash
     * @param data Data to hash
     * @return SHA-256 hash
     */
    static std::string sha256(const std::string& data);
    
    /**
     * Compute SHA-256 hash as hex string
     * @param data Data to hash
     * @return SHA-256 hash as hex
     */
    static std::string sha256Hex(const std::string& data);
    
    /**
     * Encode data to Base64
     * @param data Data to encode
     * @return Base64 encoded string
     */
    static std::string base64Encode(const std::string& data);
    
    /**
     * Decode Base64 data
     * @param encoded Base64 encoded string
     * @return Decoded data
     */
    static std::optional<std::string> base64Decode(const std::string& encoded);
    
    /**
     * Encode data to hex string
     * @param data Data to encode
     * @return Hex encoded string
     */
    static std::string hexEncode(const std::string& data);
    
    /**
     * Decode hex string
     * @param hex Hex encoded string
     * @return Decoded data
     */
    static std::optional<std::string> hexDecode(const std::string& hex);

private:
    /**
     * Generate random bytes
     * @param length Number of bytes
     * @return Random bytes
     */
    static std::vector<uint8_t> randomBytes(size_t length);
};

} // namespace nexus::core::auth
