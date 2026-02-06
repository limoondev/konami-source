/**
 * AuthManager.cpp
 * 
 * Implementation of the multi-account authentication manager.
 */

#include "AuthManager.hpp"
#include "../Logger.hpp"
#include "../Config.hpp"
#include "../EventBus.hpp"
#include "../../utils/PathUtils.hpp"

#include <algorithm>
#include <fstream>

namespace nexus::core::auth {

AuthManager::AuthManager()
    : m_microsoftAuth(std::make_unique<MicrosoftAuth>())
    , m_tokenStorage(std::make_unique<TokenStorage>()) {
}

AuthManager::~AuthManager() {
    shutdown();
}

void AuthManager::initialize() {
    if (m_initialized) return;
    
    Logger::instance().info("Initializing AuthManager");
    
    // Initialize token storage
    auto storagePath = utils::PathUtils::getAppDataPath() / "NexusLauncher" / "accounts";
    m_tokenStorage->initialize(storagePath.string());
    
    // Load saved accounts
    loadAccounts();
    
    // Get last active account from config
    m_activeAccountUuid = Config::instance().get<std::string>("auth.activeAccount", "");
    
    // Verify active account exists
    if (!m_activeAccountUuid.empty()) {
        auto it = std::find_if(m_accounts.begin(), m_accounts.end(),
            [this](const models::Account& acc) {
                return acc.uuid == m_activeAccountUuid;
            });
        
        if (it == m_accounts.end()) {
            m_activeAccountUuid.clear();
        }
    }
    
    m_initialized = true;
    Logger::instance().info("AuthManager initialized with {} accounts", m_accounts.size());
}

void AuthManager::shutdown() {
    if (!m_initialized) return;
    
    Logger::instance().info("Shutting down AuthManager");
    
    saveAccounts();
    
    // Save active account to config
    Config::instance().set("auth.activeAccount", m_activeAccountUuid);
    
    m_initialized = false;
}

std::future<std::optional<models::Account>> AuthManager::addMicrosoftAccount(
    MicrosoftAuth::DeviceCodeCallback onDeviceCode,
    MicrosoftAuth::AuthProgressCallback onProgress,
    MicrosoftAuth::AuthCompleteCallback onComplete
) {
    return std::async(std::launch::async, [this, onDeviceCode, onProgress, onComplete]()
        -> std::optional<models::Account> {
        
        auto authFuture = m_microsoftAuth->authenticateDeviceCode(
            onDeviceCode,
            onProgress,
            [](bool success, const std::string& error) {
                // Internal handling
            }
        );
        
        auto result = authFuture.get();
        
        if (!result) {
            if (onComplete) onComplete(false, m_microsoftAuth->getLastError());
            return std::nullopt;
        }
        
        // Create account
        models::Account account;
        account.uuid = result->uuid;
        account.username = result->username;
        account.type = models::AccountType::Microsoft;
        account.accessToken = result->accessToken;
        account.tokenExpiry = result->expiryTime;
        
        // Store refresh token securely
        auto oauthToken = m_microsoftAuth->getOAuthToken();
        if (oauthToken) {
            account.refreshToken = oauthToken->refreshToken;
            m_tokenStorage->storeToken(account.uuid, oauthToken->refreshToken);
        }
        
        // Add to accounts
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            
            // Check if account already exists
            auto it = std::find_if(m_accounts.begin(), m_accounts.end(),
                [&account](const models::Account& acc) {
                    return acc.uuid == account.uuid;
                });
            
            if (it != m_accounts.end()) {
                // Update existing account
                *it = account;
            } else {
                // Add new account
                m_accounts.push_back(account);
            }
            
            // Set as active if first account
            if (m_activeAccountUuid.empty()) {
                m_activeAccountUuid = account.uuid;
            }
        }
        
        saveAccounts();
        notifyAccountChange();
        
        EventBus::instance().emit("auth.accountAdded", {
            {"uuid", account.uuid},
            {"username", account.username}
        });
        
        if (onComplete) onComplete(true, "");
        
        Logger::instance().info("Added Microsoft account: {}", account.username);
        
        return account;
    });
}

bool AuthManager::removeAccount(const std::string& uuid) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = std::find_if(m_accounts.begin(), m_accounts.end(),
        [&uuid](const models::Account& acc) {
            return acc.uuid == uuid;
        });
    
    if (it == m_accounts.end()) {
        return false;
    }
    
    std::string username = it->username;
    m_accounts.erase(it);
    
    // Remove stored token
    m_tokenStorage->removeToken(uuid);
    
    // Update active account if necessary
    if (m_activeAccountUuid == uuid) {
        m_activeAccountUuid = m_accounts.empty() ? "" : m_accounts.front().uuid;
    }
    
    saveAccounts();
    notifyAccountChange();
    
    EventBus::instance().emit("auth.accountRemoved", {
        {"uuid", uuid},
        {"username", username}
    });
    
    Logger::instance().info("Removed account: {}", username);
    
    return true;
}

std::vector<models::Account> AuthManager::getAccounts() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_accounts;
}

std::optional<models::Account> AuthManager::getAccount(const std::string& uuid) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = std::find_if(m_accounts.begin(), m_accounts.end(),
        [&uuid](const models::Account& acc) {
            return acc.uuid == uuid;
        });
    
    if (it != m_accounts.end()) {
        return *it;
    }
    
    return std::nullopt;
}

std::optional<models::Account> AuthManager::getActiveAccount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_activeAccountUuid.empty()) {
        return std::nullopt;
    }
    
    auto it = std::find_if(m_accounts.begin(), m_accounts.end(),
        [this](const models::Account& acc) {
            return acc.uuid == m_activeAccountUuid;
        });
    
    if (it != m_accounts.end()) {
        return *it;
    }
    
    return std::nullopt;
}

bool AuthManager::setActiveAccount(const std::string& uuid) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = std::find_if(m_accounts.begin(), m_accounts.end(),
        [&uuid](const models::Account& acc) {
            return acc.uuid == uuid;
        });
    
    if (it == m_accounts.end()) {
        return false;
    }
    
    m_activeAccountUuid = uuid;
    
    notifyAccountChange();
    
    EventBus::instance().emit("auth.accountSwitched", {
        {"uuid", uuid},
        {"username", it->username}
    });
    
    Logger::instance().info("Switched to account: {}", it->username);
    
    return true;
}

bool AuthManager::isAuthenticated() const {
    auto account = getActiveAccount();
    return account.has_value() && !account->accessToken.empty();
}

std::future<bool> AuthManager::refreshActiveAccount(
    MicrosoftAuth::AuthProgressCallback onProgress
) {
    return std::async(std::launch::async, [this, onProgress]() -> bool {
        auto account = getActiveAccount();
        
        if (!account) {
            Logger::instance().warn("No active account to refresh");
            return false;
        }
        
        // Get refresh token from secure storage
        auto refreshToken = m_tokenStorage->getToken(account->uuid);
        if (!refreshToken) {
            refreshToken = account->refreshToken;
        }
        
        if (!refreshToken || refreshToken->empty()) {
            Logger::instance().warn("No refresh token available");
            return false;
        }
        
        auto authFuture = m_microsoftAuth->refreshAuthentication(*refreshToken, onProgress);
        auto result = authFuture.get();
        
        if (!result) {
            Logger::instance().error("Failed to refresh token: {}", m_microsoftAuth->getLastError());
            return false;
        }
        
        // Update account
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            
            auto it = std::find_if(m_accounts.begin(), m_accounts.end(),
                [&account](const models::Account& acc) {
                    return acc.uuid == account->uuid;
                });
            
            if (it != m_accounts.end()) {
                it->accessToken = result->accessToken;
                it->tokenExpiry = result->expiryTime;
                
                // Update stored refresh token
                auto oauthToken = m_microsoftAuth->getOAuthToken();
                if (oauthToken) {
                    it->refreshToken = oauthToken->refreshToken;
                    m_tokenStorage->storeToken(it->uuid, oauthToken->refreshToken);
                }
            }
        }
        
        saveAccounts();
        notifyAccountChange();
        
        Logger::instance().info("Refreshed token for account: {}", account->username);
        
        return true;
    });
}

bool AuthManager::restoreSession() {
    if (m_accounts.empty()) {
        return false;
    }
    
    auto account = getActiveAccount();
    if (!account) {
        return false;
    }
    
    // Check if token needs refresh
    if (needsRefresh(*account)) {
        Logger::instance().info("Token needs refresh, refreshing...");
        auto future = refreshActiveAccount(nullptr);
        return future.get();
    }
    
    return true;
}

std::optional<std::string> AuthManager::getAccessToken() const {
    auto account = getActiveAccount();
    if (account && !account->accessToken.empty()) {
        return account->accessToken;
    }
    return std::nullopt;
}

void AuthManager::onAccountChange(AccountChangeCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_changeCallbacks.push_back(std::move(callback));
}

void AuthManager::cancelAuthentication() {
    m_microsoftAuth->cancelAuthentication();
}

void AuthManager::loadAccounts() {
    auto accountsPath = utils::PathUtils::getAppDataPath() / "NexusLauncher" / "accounts.json";
    
    if (!std::filesystem::exists(accountsPath)) {
        return;
    }
    
    try {
        std::ifstream file(accountsPath);
        auto json = nlohmann::json::parse(file);
        
        for (const auto& accJson : json["accounts"]) {
            models::Account account;
            account.uuid = accJson["uuid"].get<std::string>();
            account.username = accJson["username"].get<std::string>();
            account.type = static_cast<models::AccountType>(accJson["type"].get<int>());
            account.accessToken = accJson.value("accessToken", "");
            
            // Load refresh token from secure storage
            auto refreshToken = m_tokenStorage->getToken(account.uuid);
            if (refreshToken) {
                account.refreshToken = *refreshToken;
            }
            
            m_accounts.push_back(account);
        }
        
    } catch (const std::exception& e) {
        Logger::instance().error("Failed to load accounts: {}", e.what());
    }
}

void AuthManager::saveAccounts() {
    auto accountsPath = utils::PathUtils::getAppDataPath() / "NexusLauncher" / "accounts.json";
    
    try {
        nlohmann::json json;
        json["accounts"] = nlohmann::json::array();
        
        for (const auto& account : m_accounts) {
            nlohmann::json accJson;
            accJson["uuid"] = account.uuid;
            accJson["username"] = account.username;
            accJson["type"] = static_cast<int>(account.type);
            // Don't save tokens to file - use secure storage
            
            json["accounts"].push_back(accJson);
        }
        
        std::filesystem::create_directories(accountsPath.parent_path());
        std::ofstream file(accountsPath);
        file << json.dump(4);
        
    } catch (const std::exception& e) {
        Logger::instance().error("Failed to save accounts: {}", e.what());
    }
}

void AuthManager::notifyAccountChange() {
    auto account = getActiveAccount();
    
    for (const auto& callback : m_changeCallbacks) {
        try {
            callback(account);
        } catch (const std::exception& e) {
            Logger::instance().error("Account change callback error: {}", e.what());
        }
    }
}

bool AuthManager::needsRefresh(const models::Account& account) const {
    if (account.accessToken.empty()) {
        return true;
    }
    
    // Refresh 5 minutes before expiry
    auto now = std::chrono::system_clock::now();
    auto buffer = std::chrono::minutes(5);
    
    return now >= (account.tokenExpiry - buffer);
}

} // namespace nexus::core::auth
