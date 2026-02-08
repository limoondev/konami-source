/**
 * TokenStorage.cpp
 * 
 * Secure token storage implementation with AES-256 encryption.
 */

#include "TokenStorage.hpp"
#include "Encryption.hpp"
#include "../Logger.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <random>

#ifdef _WIN32
#include <windows.h>
#include <wincred.h>
#pragma comment(lib, "advapi32.lib")
#elif defined(__APPLE__)
#include <Security/Security.h>
#elif defined(__linux__)
// libsecret would be used here for Linux
#endif

namespace konami::core::auth {

using json = nlohmann::json;

TokenStorage::TokenStorage() = default;
TokenStorage::~TokenStorage() {
    if (m_initialized && !m_useKeychain) {
        saveToFile();
    }
}

bool TokenStorage::initialize(const std::string& storagePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_storagePath = storagePath;
    
    // Check if platform keychain is available
    m_useKeychain = isKeychainAvailable();
    
    if (!m_useKeychain) {
        // Create storage directory
        std::filesystem::create_directories(m_storagePath);
        
        // Load existing tokens
        loadFromFile();
    }
    
    m_initialized = true;
    Logger::instance().info("TokenStorage initialized (keychain: {})", m_useKeychain);
    
    return true;
}

bool TokenStorage::storeToken(const std::string& key, const std::string& token) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_initialized) {
        Logger::instance().error("TokenStorage not initialized");
        return false;
    }
    
    if (m_useKeychain) {
        return storeInKeychain(key, token);
    } else {
        return storeInFile(key, token);
    }
}

std::optional<std::string> TokenStorage::getToken(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_initialized) {
        return std::nullopt;
    }
    
    if (m_useKeychain) {
        return getFromKeychain(key);
    } else {
        return getFromFile(key);
    }
}

bool TokenStorage::removeToken(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_initialized) {
        return false;
    }
    
    if (m_useKeychain) {
        return removeFromKeychain(key);
    } else {
        return removeFromFile(key);
    }
}

bool TokenStorage::hasToken(const std::string& key) const {
    return getToken(key).has_value();
}

void TokenStorage::clearAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_useKeychain) {
        // Clear from keychain - would need to iterate stored keys
    } else {
        m_tokens.clear();
        saveToFile();
    }
}

bool TokenStorage::isKeychainAvailable() {
#ifdef _WIN32
    return true; // Windows Credential Manager
#elif defined(__APPLE__)
    return true; // macOS Keychain
#else
    return false; // Linux - would need libsecret
#endif
}

std::string TokenStorage::encrypt(const std::string& plaintext) const {
    return Encryption::encrypt(plaintext, getEncryptionKey());
}

std::optional<std::string> TokenStorage::decrypt(const std::string& ciphertext) const {
    return Encryption::decrypt(ciphertext, getEncryptionKey());
}

std::string TokenStorage::getEncryptionKey() const {
    // In production, this should be derived from a master key stored securely
    // or use platform-specific key derivation
    
    auto keyPath = m_storagePath / ".key";
    
    if (std::filesystem::exists(keyPath)) {
        std::ifstream file(keyPath, std::ios::binary);
        std::string key((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
        return key;
    }
    
    // Generate new key
    std::string key(32, '\0');
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    for (auto& c : key) {
        c = static_cast<char>(dis(gen));
    }
    
    // Save key
    std::filesystem::create_directories(m_storagePath);
    std::ofstream file(keyPath, std::ios::binary);
    file.write(key.data(), key.size());
    
    return key;
}

bool TokenStorage::storeInKeychain(const std::string& key, const std::string& token) {
#ifdef _WIN32
    CREDENTIALW cred = {};
    cred.Type = CRED_TYPE_GENERIC;
    
    std::wstring targetName = L"KonamiClient:" + std::wstring(key.begin(), key.end());
    cred.TargetName = const_cast<LPWSTR>(targetName.c_str());
    
    cred.CredentialBlobSize = static_cast<DWORD>(token.size());
    cred.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(token.data()));
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
    
    std::wstring username = L"KonamiClient";
    cred.UserName = const_cast<LPWSTR>(username.c_str());
    
    return CredWriteW(&cred, 0) == TRUE;
    
#elif defined(__APPLE__)
    CFStringRef service = CFStringCreateWithCString(nullptr, SERVICE_NAME, kCFStringEncodingUTF8);
    CFStringRef account = CFStringCreateWithCString(nullptr, key.c_str(), kCFStringEncodingUTF8);
    CFDataRef password = CFDataCreate(nullptr, 
        reinterpret_cast<const UInt8*>(token.data()), 
        static_cast<CFIndex>(token.size()));
    
    // Delete existing item first
    const void* delKeys[] = { kSecClass, kSecAttrService, kSecAttrAccount };
    const void* delVals[] = { kSecClassGenericPassword, service, account };
    CFDictionaryRef delQuery = CFDictionaryCreate(nullptr, delKeys, delVals, 3,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    SecItemDelete(delQuery);
    CFRelease(delQuery);
    
    // Add new item
    const void* addKeys[] = { kSecClass, kSecAttrService, kSecAttrAccount, kSecValueData };
    const void* addVals[] = { kSecClassGenericPassword, service, account, password };
    CFDictionaryRef addAttrs = CFDictionaryCreate(nullptr, addKeys, addVals, 4,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    OSStatus status = SecItemAdd(addAttrs, nullptr);
    
    CFRelease(addAttrs);
    CFRelease(service);
    CFRelease(account);
    CFRelease(password);
    
    return status == errSecSuccess;
#else
    return false;
#endif
}

std::optional<std::string> TokenStorage::getFromKeychain(const std::string& key) const {
#ifdef _WIN32
    std::wstring targetName = L"KonamiClient:" + std::wstring(key.begin(), key.end());
    
    PCREDENTIALW cred = nullptr;
    if (CredReadW(targetName.c_str(), CRED_TYPE_GENERIC, 0, &cred)) {
        std::string token(reinterpret_cast<char*>(cred->CredentialBlob), 
                         cred->CredentialBlobSize);
        CredFree(cred);
        return token;
    }
    
    return std::nullopt;
    
#elif defined(__APPLE__)
    CFStringRef service = CFStringCreateWithCString(nullptr, SERVICE_NAME, kCFStringEncodingUTF8);
    CFStringRef account = CFStringCreateWithCString(nullptr, key.c_str(), kCFStringEncodingUTF8);
    
    const void* queryKeys[] = { kSecClass, kSecAttrService, kSecAttrAccount, kSecReturnData };
    const void* queryVals[] = { kSecClassGenericPassword, service, account, kCFBooleanTrue };
    CFDictionaryRef query = CFDictionaryCreate(nullptr, queryKeys, queryVals, 4,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    CFDataRef data = nullptr;
    OSStatus status = SecItemCopyMatching(query, reinterpret_cast<CFTypeRef*>(&data));
    
    CFRelease(query);
    CFRelease(service);
    CFRelease(account);
    
    if (status == errSecSuccess && data) {
        std::string token(reinterpret_cast<const char*>(CFDataGetBytePtr(data)),
                         static_cast<size_t>(CFDataGetLength(data)));
        CFRelease(data);
        return token;
    }
    
    return std::nullopt;
#else
    return std::nullopt;
#endif
}

bool TokenStorage::removeFromKeychain(const std::string& key) {
#ifdef _WIN32
    std::wstring targetName = L"KonamiClient:" + std::wstring(key.begin(), key.end());
    return CredDeleteW(targetName.c_str(), CRED_TYPE_GENERIC, 0) == TRUE;
    
#elif defined(__APPLE__)
    CFStringRef service = CFStringCreateWithCString(nullptr, SERVICE_NAME, kCFStringEncodingUTF8);
    CFStringRef account = CFStringCreateWithCString(nullptr, key.c_str(), kCFStringEncodingUTF8);
    
    const void* queryKeys[] = { kSecClass, kSecAttrService, kSecAttrAccount };
    const void* queryVals[] = { kSecClassGenericPassword, service, account };
    CFDictionaryRef query = CFDictionaryCreate(nullptr, queryKeys, queryVals, 3,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    OSStatus status = SecItemDelete(query);
    
    CFRelease(query);
    CFRelease(service);
    CFRelease(account);
    
    return status == errSecSuccess;
#else
    return false;
#endif
}

bool TokenStorage::storeInFile(const std::string& key, const std::string& token) {
    // Store plaintext in memory; saveToFile() handles encryption of the whole storage
    m_tokens[key] = token;
    saveToFile();
    return true;
}

std::optional<std::string> TokenStorage::getFromFile(const std::string& key) const {
    // Tokens are stored as plaintext in memory; loadFromFile() handles decryption
    auto it = m_tokens.find(key);
    if (it != m_tokens.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool TokenStorage::removeFromFile(const std::string& key) {
    auto it = m_tokens.find(key);
    if (it != m_tokens.end()) {
        m_tokens.erase(it);
        saveToFile();
        return true;
    }
    return false;
}

void TokenStorage::loadFromFile() {
    auto filePath = m_storagePath / "tokens.enc";
    
    if (!std::filesystem::exists(filePath)) {
        return;
    }
    
    try {
        std::ifstream file(filePath, std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        
        auto decrypted = decrypt(content);
        if (!decrypted) {
            Logger::instance().warn("Failed to decrypt token storage");
            return;
        }
        
        auto jsonData = json::parse(*decrypted);
        for (const auto& [key, value] : jsonData.items()) {
            // Tokens are stored as plaintext inside the encrypted file
            m_tokens[key] = value.get<std::string>();
        }
        
    } catch (const std::exception& e) {
        Logger::instance().error("Failed to load tokens: {}", e.what());
    }
}

void TokenStorage::saveToFile() const {
    auto filePath = m_storagePath / "tokens.enc";
    
    try {
        json jsonData;
        for (const auto& [key, value] : m_tokens) {
            jsonData[key] = value;
        }
        
        std::string encrypted = encrypt(jsonData.dump());
        
        std::filesystem::create_directories(m_storagePath);
        std::ofstream file(filePath, std::ios::binary);
        file.write(encrypted.data(), encrypted.size());
        
    } catch (const std::exception& e) {
        Logger::instance().error("Failed to save tokens: {}", e.what());
    }
}

} // namespace konami::core::auth
