#pragma once

/**
 * Application.hpp
 * 
 * Core application class that manages the lifecycle of the launcher.
 * Coordinates all subsystems including auth, downloads, mods, and game launching.
 */

#include <memory>
#include <atomic>
#include <string>
#include <functional>
#include <vector>
#include <mutex>

// Forward declarations in correct namespaces
namespace konami::core::auth { class AuthManager; }
namespace konami::core::downloader { class DownloadManager; }
namespace konami::mods { class ModManager; }
namespace konami::profile { class ProfileManager; }
namespace konami::skin { class SkinManager; }

namespace konami::core {

// Forward declarations for unimplemented managers
class VersionManager;
class ThemeManager;
class PluginManager;

/**
 * Application state enum
 */
enum class AppState {
    Uninitialized,
    Initializing,
    Ready,
    Launching,
    Running,
    ShuttingDown,
    Error
};

/**
 * Main application class
 * 
 * Singleton pattern implementation for central application management.
 * Handles initialization, shutdown, and coordination of all subsystems.
 */
class Application {
public:
    /**
     * Constructor
     */
    Application();
    
    /**
     * Destructor
     */
    ~Application();
    
    // Disable copy and move
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;
    
    /**
     * Initialize all application subsystems
     * @return true if initialization successful
     */
    bool initialize();
    
    /**
     * Shutdown the application gracefully
     */
    void shutdown();
    
    /**
     * Get current application state
     * @return Current AppState
     */
    AppState getState() const { return m_state.load(); }
    
    /**
     * Check if application is running
     * @return true if in Ready or Running state
     */
    bool isRunning() const;
    
    /**
     * Get auth manager instance
     * @return Shared pointer to AuthManager
     */
    std::shared_ptr<auth::AuthManager> getAuthManager() const { return m_authManager; }
    
    /**
     * Get download manager instance
     * @return Shared pointer to DownloadManager
     */
    std::shared_ptr<downloader::DownloadManager> getDownloadManager() const { return m_downloadManager; }
    
    /**
     * Get mod manager instance
     * @return Shared pointer to ModManager
     */
    std::shared_ptr<::konami::mods::ModManager> getModManager() const { return m_modManager; }
    
    /**
     * Get profile manager instance
     * @return Shared pointer to ProfileManager
     */
    std::shared_ptr<::konami::profile::ProfileManager> getProfileManager() const { return m_profileManager; }
    
    /**
     * Get version manager instance
     * @return Shared pointer to VersionManager
     */
    std::shared_ptr<VersionManager> getVersionManager() const { return m_versionManager; }
    
    /**
     * Get skin manager instance
     * @return Shared pointer to SkinManager
     */
    std::shared_ptr<::konami::skin::SkinManager> getSkinManager() const { return m_skinManager; }
    
    /**
     * Get theme manager instance
     * @return Shared pointer to ThemeManager
     */
    std::shared_ptr<ThemeManager> getThemeManager() const { return m_themeManager; }
    
    /**
     * Get plugin manager instance
     * @return Shared pointer to PluginManager
     */
    std::shared_ptr<PluginManager> getPluginManager() const { return m_pluginManager; }
    
    /**
     * Launch Minecraft with specified profile
     * @param profileId Profile ID to launch
     * @return true if launch successful
     */
    bool launchGame(const std::string& profileId);
    
    /**
     * Stop running game instance
     */
    void stopGame();
    
    /**
     * Register state change callback
     * @param callback Function to call on state change
     */
    void onStateChange(std::function<void(AppState)> callback);
    
    /**
     * Get application version string
     * @return Version string
     */
    static std::string getVersion() { return "1.0.0"; }
    
    /**
     * Get application name
     * @return Application name
     */
    static std::string getName() { return "KonamiClient"; }

private:
    /**
     * Set application state and notify listeners
     * @param state New state
     */
    void setState(AppState state);
    
    /**
     * Initialize authentication subsystem
     * @return true if successful
     */
    bool initializeAuth();
    
    /**
     * Initialize download subsystem
     * @return true if successful
     */
    bool initializeDownloader();
    
    /**
     * Initialize mod management subsystem
     * @return true if successful
     */
    bool initializeModManager();
    
    /**
     * Initialize profile management
     * @return true if successful
     */
    bool initializeProfileManager();
    
    /**
     * Initialize version management
     * @return true if successful
     */
    bool initializeVersionManager();
    
    /**
     * Initialize skin subsystem
     * @return true if successful
     */
    bool initializeSkinManager();
    
    /**
     * Initialize theme subsystem
     * @return true if successful
     */
    bool initializeThemeManager();
    
    /**
     * Initialize plugin subsystem
     * @return true if successful
     */
    bool initializePluginManager();

private:
    // Application state
    std::atomic<AppState> m_state{AppState::Uninitialized};
    
    // State change callbacks
    std::vector<std::function<void(AppState)>> m_stateCallbacks;
    std::mutex m_callbackMutex;
    
    // Subsystem managers
    std::shared_ptr<auth::AuthManager> m_authManager;
    std::shared_ptr<downloader::DownloadManager> m_downloadManager;
    std::shared_ptr<::konami::mods::ModManager> m_modManager;
    std::shared_ptr<::konami::profile::ProfileManager> m_profileManager;
    std::shared_ptr<VersionManager> m_versionManager;
    std::shared_ptr<::konami::skin::SkinManager> m_skinManager;
    std::shared_ptr<ThemeManager> m_themeManager;
    std::shared_ptr<PluginManager> m_pluginManager;
    
    // Game process handle (platform-specific)
    void* m_gameProcess{nullptr};
};

} // namespace konami::core
