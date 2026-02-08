#pragma once

/**
 * MicrosoftAuth.hpp
 * 
 * Microsoft OAuth2 authentication flow for Minecraft accounts.
 * Implements device code flow for secure authentication.
 */

#include <string>
#include <functional>
#include <optional>
#include <chrono>
#include <future>
#include <atomic>

namespace konami::core::auth {

/**
 * OAuth2 token structure
 */
struct OAuthToken {
    std::string accessToken;
    std::string refreshToken;
    std::string tokenType;
    int expiresIn{0};
    std::chrono::system_clock::time_point expiryTime;
    
    bool isExpired() const {
        return std::chrono::system_clock::now() >= expiryTime;
    }
    
    bool needsRefresh() const {
        // Refresh 5 minutes before expiry
        auto buffer = std::chrono::minutes(5);
        return std::chrono::system_clock::now() >= (expiryTime - buffer);
    }
};

/**
 * Xbox Live token structure
 */
struct XboxToken {
    std::string token;
    std::string userHash;
    std::chrono::system_clock::time_point expiryTime;
};

/**
 * Minecraft authentication result
 */
struct MinecraftAuthResult {
    std::string accessToken;
    std::string uuid;
    std::string username;
    std::chrono::system_clock::time_point expiryTime;
    bool hasGamePass{false};
};

/**
 * Device code for user authentication
 */
struct DeviceCode {
    std::string deviceCode;
    std::string userCode;
    std::string verificationUri;
    std::string verificationUriComplete;
    int expiresIn{0};
    int interval{5};
};

/**
 * Authentication callback types
 */
using AuthProgressCallback = std::function<void(const std::string& status, float progress)>;
using DeviceCodeCallback = std::function<void(const DeviceCode& code)>;
using AuthCompleteCallback = std::function<void(bool success, const std::string& error)>;

/**
 * MicrosoftAuth - OAuth2 authentication with Microsoft/Xbox Live/Minecraft
 * 
 * Authentication flow:
 * 1. Get device code from Microsoft
 * 2. User enters code at Microsoft website
 * 3. Poll for OAuth token
 * 4. Exchange for Xbox Live token
 * 5. Get XSTS token
 * 6. Authenticate with Minecraft
 * 7. Verify game ownership
 */
class MicrosoftAuth {
public:
    // Microsoft OAuth endpoints
    static constexpr const char* MICROSOFT_AUTH_URL = 
        "https://login.microsoftonline.com/consumers/oauth2/v2.0/authorize";
    static constexpr const char* MICROSOFT_TOKEN_URL = 
        "https://login.microsoftonline.com/consumers/oauth2/v2.0/token";
    static constexpr const char* MICROSOFT_DEVICE_CODE_URL = 
        "https://login.microsoftonline.com/consumers/oauth2/v2.0/devicecode";
    
    // Xbox Live endpoints
    static constexpr const char* XBOX_AUTH_URL = 
        "https://user.auth.xboxlive.com/user/authenticate";
    static constexpr const char* XBOX_XSTS_URL = 
        "https://xsts.auth.xboxlive.com/xsts/authorize";
    
    // Minecraft endpoints
    static constexpr const char* MINECRAFT_AUTH_URL = 
        "https://api.minecraftservices.com/authentication/login_with_xbox";
    static constexpr const char* MINECRAFT_PROFILE_URL = 
        "https://api.minecraftservices.com/minecraft/profile";
    static constexpr const char* MINECRAFT_OWNERSHIP_URL = 
        "https://api.minecraftservices.com/entitlements/mcstore";
    
    // Azure App Client ID (placeholder - should use your own)
    static constexpr const char* CLIENT_ID = "00000000-0000-0000-0000-000000000000";
    static constexpr const char* SCOPE = "XboxLive.signin offline_access";
    
    /**
     * Constructor
     */
    MicrosoftAuth();
    
    /**
     * Destructor
     */
    ~MicrosoftAuth();
    
    /**
     * Start device code authentication flow
     * @param onDeviceCode Callback when device code is received
     * @param onProgress Progress callback
     * @param onComplete Completion callback
     * @return Future for the auth result
     */
    std::future<std::optional<MinecraftAuthResult>> authenticateDeviceCode(
        DeviceCodeCallback onDeviceCode,
        AuthProgressCallback onProgress = nullptr,
        AuthCompleteCallback onComplete = nullptr
    );
    
    /**
     * Refresh an existing token
     * @param refreshToken Refresh token to use
     * @param onProgress Progress callback
     * @return Future for the auth result
     */
    std::future<std::optional<MinecraftAuthResult>> refreshAuthentication(
        const std::string& refreshToken,
        AuthProgressCallback onProgress = nullptr
    );
    
    /**
     * Cancel ongoing authentication
     */
    void cancelAuthentication();
    
    /**
     * Check if authentication is in progress
     * @return true if authenticating
     */
    bool isAuthenticating() const { return m_authenticating; }
    
    /**
     * Get last error message
     * @return Error message
     */
    std::string getLastError() const { return m_lastError; }
    
    /**
     * Get current OAuth token
     * @return OAuth token if available
     */
    std::optional<OAuthToken> getOAuthToken() const { return m_oauthToken; }

private:
    /**
     * Request device code from Microsoft
     * @return Device code info
     */
    std::optional<DeviceCode> requestDeviceCode();
    
    /**
     * Poll for OAuth token after user enters code
     * @param deviceCode Device code info
     * @param onProgress Progress callback
     * @return OAuth token
     */
    std::optional<OAuthToken> pollForToken(
        const DeviceCode& deviceCode,
        AuthProgressCallback onProgress
    );
    
    /**
     * Exchange OAuth token for Xbox Live token
     * @param oauthToken OAuth token
     * @return Xbox Live token
     */
    std::optional<XboxToken> authenticateXboxLive(const OAuthToken& oauthToken);
    
    /**
     * Get XSTS token from Xbox Live token
     * @param xboxToken Xbox Live token
     * @return XSTS token
     */
    std::optional<XboxToken> getXSTSToken(const XboxToken& xboxToken);
    
    /**
     * Authenticate with Minecraft using XSTS token
     * @param xstsToken XSTS token
     * @return Minecraft auth result
     */
    std::optional<MinecraftAuthResult> authenticateMinecraft(const XboxToken& xstsToken);
    
    /**
     * Verify game ownership
     * @param accessToken Minecraft access token
     * @return true if user owns Minecraft
     */
    bool verifyGameOwnership(const std::string& accessToken);
    
    /**
     * Get Minecraft profile
     * @param accessToken Minecraft access token
     * @return Profile info (uuid, username)
     */
    std::optional<std::pair<std::string, std::string>> getMinecraftProfile(
        const std::string& accessToken
    );
    
    /**
     * Refresh OAuth token
     * @param refreshToken Refresh token
     * @return New OAuth token
     */
    std::optional<OAuthToken> refreshOAuthToken(const std::string& refreshToken);

private:
    std::atomic<bool> m_authenticating{false};
    std::atomic<bool> m_cancelled{false};
    std::string m_lastError;
    std::optional<OAuthToken> m_oauthToken;
};

} // namespace konami::core::auth
