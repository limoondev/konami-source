// Konami Client - UI Bridge
// Connects C++ backend with Slint UI

#pragma once

#include <slint.h>
#include <memory>
#include <functional>
#include <string>
#include <vector>

// Forward declarations
namespace konami {
    class Application;
    class AuthManager;
    class ProfileManager;
    class ModManager;
    class DownloadManager;
    class GameLauncher;
    class SkinEngine;
}

// Include generated Slint header
#include "main-window.h"

namespace konami::ui {

/**
 * @brief Bridge between C++ backend and Slint UI
 * 
 * Handles all communication between the business logic
 * and the user interface, converting between C++ types
 * and Slint types.
 */
class UIBridge {
public:
    UIBridge(Application& app);
    ~UIBridge();
    
    // Initialize and run the UI
    bool initialize();
    void run();
    
    // Get main window handle
    MainWindow& window() { return *m_window; }
    
    // Update UI state
    void updateAccountInfo();
    void updateProfiles();
    void updateMods();
    void updateSkins();
    void updateDownloadProgress(float progress, const std::string& currentFile);
    void updateGameStatus(bool running, const std::string& memoryUsage);
    void updateNews();
    void updateSettings();
    
    // Show dialogs
    void showError(const std::string& title, const std::string& message);
    void showInfo(const std::string& title, const std::string& message);
    void showConfirm(const std::string& title, const std::string& message,
                     std::function<void(bool)> callback);
    
    // Navigate to page
    void navigateTo(const std::string& page);
    
private:
    // Setup callbacks
    void setupNavigationCallbacks();
    void setupAuthCallbacks();
    void setupProfileCallbacks();
    void setupModCallbacks();
    void setupSkinCallbacks();
    void setupSettingsCallbacks();
    void setupLaunchCallbacks();
    void setupWindowCallbacks();
    
    // Convert types
    AccountInfo toSlintAccount(const struct Account& account);
    ProfileInfo toSlintProfile(const struct Profile& profile);
    ModInfo toSlintMod(const struct Mod& mod);
    SkinInfo toSlintSkin(const struct Skin& skin);
    NewsItem toSlintNews(const struct NewsEntry& news);
    
    // Members
    Application& m_app;
    slint::ComponentHandle<MainWindow> m_window;
    bool m_initialized{false};
};

/**
 * @brief Account info for UI
 */
struct UIAccountInfo {
    std::string username;
    std::string uuid;
    std::string avatarUrl;
    bool isLoggedIn;
    std::string accountType;
};

/**
 * @brief Profile info for UI
 */
struct UIProfileInfo {
    std::string id;
    std::string name;
    std::string gameVersion;
    std::string loader;
    std::string loaderVersion;
    std::string icon;
    std::string lastPlayed;
    std::string totalPlaytime;
    int modCount;
    bool isFavorite;
    std::string createdAt;
};

/**
 * @brief Mod info for UI
 */
struct UIModInfo {
    std::string id;
    std::string name;
    std::string author;
    std::string description;
    std::string version;
    std::string gameVersion;
    int downloads;
    std::string iconUrl;
    bool isInstalled;
    bool isEnabled;
    bool isUpdating;
    std::string source;
    std::string category;
};

/**
 * @brief Download progress for UI
 */
struct UIDownloadProgress {
    bool isDownloading;
    std::string currentFile;
    float currentProgress;
    float totalProgress;
    std::string downloadSpeed;
    std::string eta;
    int filesCompleted;
    int filesTotal;
};

/**
 * @brief Game status for UI
 */
struct UIGameStatus {
    bool isRunning;
    std::string currentVersion;
    std::string memoryUsage;
    std::string uptime;
};

/**
 * @brief Skin info for UI
 */
struct UISkinInfo {
    std::string id;
    std::string name;
    std::string textureUrl;
    std::string modelType;
    bool isActive;
    bool isFavorite;
    std::string createdAt;
};

/**
 * @brief Cape info for UI
 */
struct UICapeInfo {
    std::string id;
    std::string name;
    std::string textureUrl;
    bool isActive;
    std::string source;
};

/**
 * @brief News item for UI
 */
struct UINewsItem {
    std::string id;
    std::string title;
    std::string summary;
    std::string imageUrl;
    std::string date;
    std::string url;
    std::string category;
};

/**
 * @brief Settings for UI
 */
struct UISettings {
    // General
    std::string language;
    bool autoUpdate;
    bool minimizeOnLaunch;
    bool closeOnLaunch;
    bool showNews;
    
    // Theme
    std::string themeName;
    uint32_t accentColor;
    bool useBlurEffects;
    float animationSpeed;
    
    // Java
    std::string javaPath;
    int minMemory;
    int maxMemory;
    std::string jvmArgs;
    
    // Performance
    int concurrentDownloads;
    bool useCache;
    
    // Advanced
    std::string gameDirectory;
    bool keepLauncherOpen;
    bool showConsole;
};

} // namespace konami::ui
