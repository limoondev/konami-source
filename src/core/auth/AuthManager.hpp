#pragma once

/**
 * AuthManager.hpp
 * 
 * Manages multiple Minecraft accounts with secure token storage.
 * Supports Microsoft authentication and multi-account switching.
 */

#include "MicrosoftAuth.hpp"
#include "TokenStorage.hpp"
#include "../../models/Account.hpp"

#include <vector>
#include <memory>
#include <optional>
#include <mutex>
#include <functional>

namespace nexus::core::auth {

/**
 * Account change callback
 */
using AccountChangeCallback = std::function<void(const std::optional<models::Account>&)>;

/**
 * AuthManager - Multi-account authentication manager
 * 
 * Features:
 * - Multiple account support
 * - Secure token storage
 * - Auto token refresh
 * - Account switching
 */
class AuthManager {
public:
    /**
     * Constructor
     */
    AuthManager();
    
    /**
     * Destructor
     */
    ~AuthManager();
    
    // Disable copy and move
    AuthManager(const AuthManager&) = delete;
    AuthManager& operator=(const AuthManager&) = delete;
    
    /**
     * Initialize the auth manager
     */
    void initialize();
    
    /**
     * Shutdown and cleanup
     */
    void shutdown();
    
    /**
     * Add a new Microsoft account
     * @param onDeviceCode Callback for device code display
     * @param onProgress Progress callback
     * @param onComplete Completion callback
     * @return Future for the new account
     */
    std::future<std::optional<models::Account>> addMicrosoftAccount(
        MicrosoftAuth::DeviceCodeCallback onDeviceCode,
        MicrosoftAuth::AuthProgressCallback onProgress = nullptr,
        MicrosoftAuth::AuthCompleteCallback onComplete = nullptr
    );
    
    /**
     * Remove an account
     * @param uuid Account UUID
     * @return true if removed
     */
    bool removeAccount(const std::string& uuid);
    
    /**
     * Get all accounts
     * @return Vector of accounts
     */
    std::vector<models::Account> getAccounts() const;
    
    /**
     * Get account by UUID
     * @param uuid Account UUID
     * @return Account if found
     */
    std::optional<models::Account> getAccount(const std::string& uuid) const;
    
    /**
     * Get current active account
     * @return Active account if any
     */
    std::optional<models::Account> getActiveAccount() const;
    
    /**
     * Set active account
     * @param uuid Account UUID
     * @return true if successful
     */
    bool setActiveAccount(const std::string& uuid);
    
    /**
     * Check if any account is authenticated
     * @return true if has authenticated account
     */
    bool isAuthenticated() const;
    
    /**
     * Refresh active account token
     * @param onProgress Progress callback
     * @return true if refresh successful
     */
    std::future<bool> refreshActiveAccount(
        MicrosoftAuth::AuthProgressCallback onProgress = nullptr
    );
    
    /**
     * Restore previous session
     * @return true if session restored
     */
    bool restoreSession();
    
    /**
     * Get access token for active account
     * @return Access token if available
     */
    std::optional<std::string> getAccessToken() const;
    
    /**
     * Register account change callback
     * @param callback Callback function
     */
    void onAccountChange(AccountChangeCallback callback);
    
    /**
     * Cancel ongoing authentication
     */
    void cancelAuthentication();

private:
    /**
     * Load accounts from storage
     */
    void loadAccounts();
    
    /**
     * Save accounts to storage
     */
    void saveAccounts();
    
    /**
     * Notify account change listeners
     */
    void notifyAccountChange();
    
    /**
     * Check if token needs refresh
     * @param account Account to check
     * @return true if needs refresh
     */
    bool needsRefresh(const models::Account& account) const;

private:
    std::unique_ptr<MicrosoftAuth> m_microsoftAuth;
    std::unique_ptr<TokenStorage> m_tokenStorage;
    
    std::vector<models::Account> m_accounts;
    std::string m_activeAccountUuid;
    
    std::vector<AccountChangeCallback> m_changeCallbacks;
    mutable std::mutex m_mutex;
    
    bool m_initialized{false};
};

} // namespace nexus::core::auth
