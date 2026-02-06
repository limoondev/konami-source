/**
 * MicrosoftAuth.cpp
 * 
 * Implementation of Microsoft OAuth2 authentication for Minecraft.
 */

#include "MicrosoftAuth.hpp"
#include "../Logger.hpp"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>

namespace konami::core::auth {

using json = nlohmann::json;

MicrosoftAuth::MicrosoftAuth() = default;
MicrosoftAuth::~MicrosoftAuth() {
    cancelAuthentication();
}

std::future<std::optional<MinecraftAuthResult>> MicrosoftAuth::authenticateDeviceCode(
    DeviceCodeCallback onDeviceCode,
    AuthProgressCallback onProgress,
    AuthCompleteCallback onComplete
) {
    return std::async(std::launch::async, [this, onDeviceCode, onProgress, onComplete]() 
        -> std::optional<MinecraftAuthResult> {
        
        m_authenticating = true;
        m_cancelled = false;
        m_lastError.clear();
        
        try {
            // Step 1: Get device code
            if (onProgress) onProgress("Requesting device code...", 0.0f);
            
            auto deviceCode = requestDeviceCode();
            if (!deviceCode) {
                if (onComplete) onComplete(false, m_lastError);
                m_authenticating = false;
                return std::nullopt;
            }
            
            // Notify user to enter code
            if (onDeviceCode) onDeviceCode(*deviceCode);
            
            // Step 2: Poll for OAuth token
            if (onProgress) onProgress("Waiting for user authentication...", 0.1f);
            
            auto oauthToken = pollForToken(*deviceCode, onProgress);
            if (!oauthToken) {
                if (onComplete) onComplete(false, m_lastError);
                m_authenticating = false;
                return std::nullopt;
            }
            
            m_oauthToken = oauthToken;
            
            // Step 3: Xbox Live authentication
            if (onProgress) onProgress("Authenticating with Xbox Live...", 0.4f);
            
            auto xboxToken = authenticateXboxLive(*oauthToken);
            if (!xboxToken) {
                if (onComplete) onComplete(false, m_lastError);
                m_authenticating = false;
                return std::nullopt;
            }
            
            // Step 4: Get XSTS token
            if (onProgress) onProgress("Getting XSTS token...", 0.6f);
            
            auto xstsToken = getXSTSToken(*xboxToken);
            if (!xstsToken) {
                if (onComplete) onComplete(false, m_lastError);
                m_authenticating = false;
                return std::nullopt;
            }
            
            // Step 5: Minecraft authentication
            if (onProgress) onProgress("Authenticating with Minecraft...", 0.8f);
            
            auto mcResult = authenticateMinecraft(*xstsToken);
            if (!mcResult) {
                if (onComplete) onComplete(false, m_lastError);
                m_authenticating = false;
                return std::nullopt;
            }
            
            // Step 6: Verify ownership
            if (onProgress) onProgress("Verifying game ownership...", 0.9f);
            
            if (!verifyGameOwnership(mcResult->accessToken)) {
                m_lastError = "User does not own Minecraft";
                if (onComplete) onComplete(false, m_lastError);
                m_authenticating = false;
                return std::nullopt;
            }
            
            // Success!
            if (onProgress) onProgress("Authentication complete!", 1.0f);
            if (onComplete) onComplete(true, "");
            
            m_authenticating = false;
            return mcResult;
            
        } catch (const std::exception& e) {
            m_lastError = e.what();
            Logger::instance().error("Authentication error: {}", e.what());
            if (onComplete) onComplete(false, m_lastError);
            m_authenticating = false;
            return std::nullopt;
        }
    });
}

std::future<std::optional<MinecraftAuthResult>> MicrosoftAuth::refreshAuthentication(
    const std::string& refreshToken,
    AuthProgressCallback onProgress
) {
    return std::async(std::launch::async, [this, refreshToken, onProgress]()
        -> std::optional<MinecraftAuthResult> {
        
        m_authenticating = true;
        m_cancelled = false;
        m_lastError.clear();
        
        try {
            // Step 1: Refresh OAuth token
            if (onProgress) onProgress("Refreshing token...", 0.1f);
            
            auto oauthToken = refreshOAuthToken(refreshToken);
            if (!oauthToken) {
                m_authenticating = false;
                return std::nullopt;
            }
            
            m_oauthToken = oauthToken;
            
            // Steps 2-5: Same as regular authentication
            if (onProgress) onProgress("Authenticating with Xbox Live...", 0.3f);
            auto xboxToken = authenticateXboxLive(*oauthToken);
            if (!xboxToken) {
                m_authenticating = false;
                return std::nullopt;
            }
            
            if (onProgress) onProgress("Getting XSTS token...", 0.5f);
            auto xstsToken = getXSTSToken(*xboxToken);
            if (!xstsToken) {
                m_authenticating = false;
                return std::nullopt;
            }
            
            if (onProgress) onProgress("Authenticating with Minecraft...", 0.7f);
            auto mcResult = authenticateMinecraft(*xstsToken);
            if (!mcResult) {
                m_authenticating = false;
                return std::nullopt;
            }
            
            if (onProgress) onProgress("Complete!", 1.0f);
            
            m_authenticating = false;
            return mcResult;
            
        } catch (const std::exception& e) {
            m_lastError = e.what();
            m_authenticating = false;
            return std::nullopt;
        }
    });
}

void MicrosoftAuth::cancelAuthentication() {
    m_cancelled = true;
}

std::optional<DeviceCode> MicrosoftAuth::requestDeviceCode() {
    try {
        cpr::Response response = cpr::Post(
            cpr::Url{MICROSOFT_DEVICE_CODE_URL},
            cpr::Payload{
                {"client_id", CLIENT_ID},
                {"scope", SCOPE}
            },
            cpr::Header{{"Content-Type", "application/x-www-form-urlencoded"}}
        );
        
        if (response.status_code != 200) {
            m_lastError = "Failed to get device code: HTTP " + std::to_string(response.status_code);
            return std::nullopt;
        }
        
        auto jsonResponse = json::parse(response.text);
        
        DeviceCode code;
        code.deviceCode = jsonResponse["device_code"].get<std::string>();
        code.userCode = jsonResponse["user_code"].get<std::string>();
        code.verificationUri = jsonResponse["verification_uri"].get<std::string>();
        code.verificationUriComplete = jsonResponse.value("verification_uri_complete", "");
        code.expiresIn = jsonResponse["expires_in"].get<int>();
        code.interval = jsonResponse.value("interval", 5);
        
        return code;
        
    } catch (const std::exception& e) {
        m_lastError = std::string("Device code error: ") + e.what();
        return std::nullopt;
    }
}

std::optional<OAuthToken> MicrosoftAuth::pollForToken(
    const DeviceCode& deviceCode,
    AuthProgressCallback onProgress
) {
    auto startTime = std::chrono::steady_clock::now();
    auto expiryTime = startTime + std::chrono::seconds(deviceCode.expiresIn);
    
    while (std::chrono::steady_clock::now() < expiryTime) {
        if (m_cancelled) {
            m_lastError = "Authentication cancelled";
            return std::nullopt;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(deviceCode.interval));
        
        try {
            cpr::Response response = cpr::Post(
                cpr::Url{MICROSOFT_TOKEN_URL},
                cpr::Payload{
                    {"client_id", CLIENT_ID},
                    {"grant_type", "urn:ietf:params:oauth:grant-type:device_code"},
                    {"device_code", deviceCode.deviceCode}
                },
                cpr::Header{{"Content-Type", "application/x-www-form-urlencoded"}}
            );
            
            auto jsonResponse = json::parse(response.text);
            
            if (response.status_code == 200) {
                OAuthToken token;
                token.accessToken = jsonResponse["access_token"].get<std::string>();
                token.refreshToken = jsonResponse["refresh_token"].get<std::string>();
                token.tokenType = jsonResponse["token_type"].get<std::string>();
                token.expiresIn = jsonResponse["expires_in"].get<int>();
                token.expiryTime = std::chrono::system_clock::now() + 
                                   std::chrono::seconds(token.expiresIn);
                return token;
            }
            
            std::string error = jsonResponse.value("error", "");
            
            if (error == "authorization_pending") {
                // User hasn't entered code yet, continue polling
                float elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - startTime
                ).count();
                float progress = 0.1f + (elapsed / deviceCode.expiresIn) * 0.3f;
                if (onProgress) onProgress("Waiting for user...", progress);
                continue;
            }
            
            if (error == "authorization_declined") {
                m_lastError = "User declined authorization";
                return std::nullopt;
            }
            
            if (error == "expired_token") {
                m_lastError = "Device code expired";
                return std::nullopt;
            }
            
            // Other error
            m_lastError = jsonResponse.value("error_description", error);
            return std::nullopt;
            
        } catch (const std::exception& e) {
            m_lastError = std::string("Token polling error: ") + e.what();
            return std::nullopt;
        }
    }
    
    m_lastError = "Device code expired";
    return std::nullopt;
}

std::optional<XboxToken> MicrosoftAuth::authenticateXboxLive(const OAuthToken& oauthToken) {
    try {
        json requestBody = {
            {"Properties", {
                {"AuthMethod", "RPS"},
                {"SiteName", "user.auth.xboxlive.com"},
                {"RpsTicket", "d=" + oauthToken.accessToken}
            }},
            {"RelyingParty", "http://auth.xboxlive.com"},
            {"TokenType", "JWT"}
        };
        
        cpr::Response response = cpr::Post(
            cpr::Url{XBOX_AUTH_URL},
            cpr::Body{requestBody.dump()},
            cpr::Header{
                {"Content-Type", "application/json"},
                {"Accept", "application/json"}
            }
        );
        
        if (response.status_code != 200) {
            m_lastError = "Xbox Live auth failed: HTTP " + std::to_string(response.status_code);
            return std::nullopt;
        }
        
        auto jsonResponse = json::parse(response.text);
        
        XboxToken token;
        token.token = jsonResponse["Token"].get<std::string>();
        token.userHash = jsonResponse["DisplayClaims"]["xui"][0]["uhs"].get<std::string>();
        
        // Parse expiry time
        std::string notAfter = jsonResponse["NotAfter"].get<std::string>();
        // ISO 8601 parsing would go here
        token.expiryTime = std::chrono::system_clock::now() + std::chrono::hours(24);
        
        return token;
        
    } catch (const std::exception& e) {
        m_lastError = std::string("Xbox Live error: ") + e.what();
        return std::nullopt;
    }
}

std::optional<XboxToken> MicrosoftAuth::getXSTSToken(const XboxToken& xboxToken) {
    try {
        json requestBody = {
            {"Properties", {
                {"SandboxId", "RETAIL"},
                {"UserTokens", json::array({xboxToken.token})}
            }},
            {"RelyingParty", "rp://api.minecraftservices.com/"},
            {"TokenType", "JWT"}
        };
        
        cpr::Response response = cpr::Post(
            cpr::Url{XBOX_XSTS_URL},
            cpr::Body{requestBody.dump()},
            cpr::Header{
                {"Content-Type", "application/json"},
                {"Accept", "application/json"}
            }
        );
        
        if (response.status_code == 401) {
            auto jsonResponse = json::parse(response.text);
            uint64_t xerr = jsonResponse.value("XErr", 0ULL);
            
            if (xerr == 2148916233) {
                m_lastError = "Xbox account not linked to Microsoft account";
            } else if (xerr == 2148916238) {
                m_lastError = "Account belongs to a minor without Xbox parental consent";
            } else {
                m_lastError = "XSTS authentication failed";
            }
            return std::nullopt;
        }
        
        if (response.status_code != 200) {
            m_lastError = "XSTS auth failed: HTTP " + std::to_string(response.status_code);
            return std::nullopt;
        }
        
        auto jsonResponse = json::parse(response.text);
        
        XboxToken token;
        token.token = jsonResponse["Token"].get<std::string>();
        token.userHash = jsonResponse["DisplayClaims"]["xui"][0]["uhs"].get<std::string>();
        token.expiryTime = std::chrono::system_clock::now() + std::chrono::hours(24);
        
        return token;
        
    } catch (const std::exception& e) {
        m_lastError = std::string("XSTS error: ") + e.what();
        return std::nullopt;
    }
}

std::optional<MinecraftAuthResult> MicrosoftAuth::authenticateMinecraft(const XboxToken& xstsToken) {
    try {
        json requestBody = {
            {"identityToken", "XBL3.0 x=" + xstsToken.userHash + ";" + xstsToken.token}
        };
        
        cpr::Response response = cpr::Post(
            cpr::Url{MINECRAFT_AUTH_URL},
            cpr::Body{requestBody.dump()},
            cpr::Header{
                {"Content-Type", "application/json"},
                {"Accept", "application/json"}
            }
        );
        
        if (response.status_code != 200) {
            m_lastError = "Minecraft auth failed: HTTP " + std::to_string(response.status_code);
            return std::nullopt;
        }
        
        auto jsonResponse = json::parse(response.text);
        
        MinecraftAuthResult result;
        result.accessToken = jsonResponse["access_token"].get<std::string>();
        result.expiryTime = std::chrono::system_clock::now() + 
                           std::chrono::seconds(jsonResponse["expires_in"].get<int>());
        
        // Get profile info
        auto profile = getMinecraftProfile(result.accessToken);
        if (profile) {
            result.uuid = profile->first;
            result.username = profile->second;
        }
        
        return result;
        
    } catch (const std::exception& e) {
        m_lastError = std::string("Minecraft auth error: ") + e.what();
        return std::nullopt;
    }
}

bool MicrosoftAuth::verifyGameOwnership(const std::string& accessToken) {
    try {
        cpr::Response response = cpr::Get(
            cpr::Url{MINECRAFT_OWNERSHIP_URL},
            cpr::Header{
                {"Authorization", "Bearer " + accessToken},
                {"Accept", "application/json"}
            }
        );
        
        if (response.status_code != 200) {
            return false;
        }
        
        auto jsonResponse = json::parse(response.text);
        auto items = jsonResponse["items"];
        
        // Check for game ownership entitlements
        for (const auto& item : items) {
            std::string name = item.value("name", "");
            if (name == "product_minecraft" || name == "game_minecraft") {
                return true;
            }
        }
        
        return false;
        
    } catch (const std::exception& e) {
        Logger::instance().warn("Ownership verification error: {}", e.what());
        return false;
    }
}

std::optional<std::pair<std::string, std::string>> MicrosoftAuth::getMinecraftProfile(
    const std::string& accessToken
) {
    try {
        cpr::Response response = cpr::Get(
            cpr::Url{MINECRAFT_PROFILE_URL},
            cpr::Header{
                {"Authorization", "Bearer " + accessToken},
                {"Accept", "application/json"}
            }
        );
        
        if (response.status_code != 200) {
            return std::nullopt;
        }
        
        auto jsonResponse = json::parse(response.text);
        
        return std::make_pair(
            jsonResponse["id"].get<std::string>(),
            jsonResponse["name"].get<std::string>()
        );
        
    } catch (const std::exception& e) {
        Logger::instance().warn("Profile fetch error: {}", e.what());
        return std::nullopt;
    }
}

std::optional<OAuthToken> MicrosoftAuth::refreshOAuthToken(const std::string& refreshToken) {
    try {
        cpr::Response response = cpr::Post(
            cpr::Url{MICROSOFT_TOKEN_URL},
            cpr::Payload{
                {"client_id", CLIENT_ID},
                {"grant_type", "refresh_token"},
                {"refresh_token", refreshToken},
                {"scope", SCOPE}
            },
            cpr::Header{{"Content-Type", "application/x-www-form-urlencoded"}}
        );
        
        if (response.status_code != 200) {
            m_lastError = "Token refresh failed: HTTP " + std::to_string(response.status_code);
            return std::nullopt;
        }
        
        auto jsonResponse = json::parse(response.text);
        
        OAuthToken token;
        token.accessToken = jsonResponse["access_token"].get<std::string>();
        token.refreshToken = jsonResponse["refresh_token"].get<std::string>();
        token.tokenType = jsonResponse["token_type"].get<std::string>();
        token.expiresIn = jsonResponse["expires_in"].get<int>();
        token.expiryTime = std::chrono::system_clock::now() + 
                          std::chrono::seconds(token.expiresIn);
        
        return token;
        
    } catch (const std::exception& e) {
        m_lastError = std::string("Token refresh error: ") + e.what();
        return std::nullopt;
    }
}

} // namespace konami::core::auth
