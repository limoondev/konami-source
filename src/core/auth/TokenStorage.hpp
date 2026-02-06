#pragma once

/**
 * TokenStorage.hpp
 * 
 * Secure token storage using AES-256 encryption.
 * Platform-specific keychain integration where available.
 */

#include <string>
#include <optional>
#include <filesystem>
#include <mutex>
#include <unordered_map>

namespace konami::core::auth {

/**
 * TokenStorage - Secure credential storage
 * 
 * Features:
 * - AES-256-GCM encryption
 * - Platform keychain integration (Windows Credential Manager, macOS Keychain, Linux Secret Service)
 * - Fallback to encrypted file storage
 */
class TokenStorage {
public:
    /**
     * Constructor
     */
    TokenStorage();
    
    /**
     * Destructor
     */
    ~TokenStorage();
    
    /**
     * Initialize storage
     * @param storagePath Path for encrypted storage files
     * @return true if successful
     */
    bool initialize(const std::string& storagePath);
    
    /**
     * Store a token securely
     * @param key Token key (e.g., account UUID)
     * @param token Token value
     * @return true if stored successfully
     */
    bool storeToken(const std::string& key, const std::string& token);
    
    /**
     * Retrieve a stored token
     * @param key Token key
     * @return Token value if found
     */
    std::optional<std::string> getToken(const std::string& key) const;
    
    /**
     * Remove a stored token
     * @param key Token key
     * @return true if removed
     */
    bool removeToken(const std::string& key);
    
    /**
     * Check if token exists
     * @param key Token key
     * @return true if exists
     */
    bool hasToken(const std::string& key) const;
    
    /**
     * Clear all stored tokens
     */
    void clearAll();
    
    /**
     * Check if platform keychain is available
     * @return true if keychain available
     */
    static bool isKeychainAvailable();

private:
    /**
     * Encrypt data using AES-256-GCM
     * @param plaintext Data to encrypt
     * @return Encrypted data (IV + ciphertext + tag)
     */
    std::string encrypt(const std::string& plaintext) const;
    
    /**
     * Decrypt data using AES-256-GCM
     * @param ciphertext Encrypted data
     * @return Decrypted data
     */
    std::optional<std::string> decrypt(const std::string& ciphertext) const;
    
    /**
     * Get or generate encryption key
     * @return Encryption key
     */
    std::string getEncryptionKey() const;
    
    /**
     * Store using platform keychain
     * @param key Token key
     * @param token Token value
     * @return true if successful
     */
    bool storeInKeychain(const std::string& key, const std::string& token);
    
    /**
     * Retrieve from platform keychain
     * @param key Token key
     * @return Token value if found
     */
    std::optional<std::string> getFromKeychain(const std::string& key) const;
    
    /**
     * Remove from platform keychain
     * @param key Token key
     * @return true if removed
     */
    bool removeFromKeychain(const std::string& key);
    
    /**
     * Store in encrypted file
     * @param key Token key
     * @param token Token value
     * @return true if successful
     */
    bool storeInFile(const std::string& key, const std::string& token);
    
    /**
     * Retrieve from encrypted file
     * @param key Token key
     * @return Token value if found
     */
    std::optional<std::string> getFromFile(const std::string& key) const;
    
    /**
     * Remove from encrypted file
     * @param key Token key
     * @return true if removed
     */
    bool removeFromFile(const std::string& key);
    
    /**
     * Load tokens from storage file
     */
    void loadFromFile();
    
    /**
     * Save tokens to storage file
     */
    void saveToFile() const;

private:
    std::filesystem::path m_storagePath;
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::string> m_tokens;
    bool m_useKeychain{false};
    bool m_initialized{false};
    
    // Service name for keychain
    static constexpr const char* SERVICE_NAME = "KonamiClient";
};

} // namespace konami::core::auth
