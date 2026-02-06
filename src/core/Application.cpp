/**
 * Application.cpp
 * 
 * Implementation of the core Application class.
 */

#include "Application.hpp"
#include "Logger.hpp"
#include "Config.hpp"
#include "EventBus.hpp"
#include "auth/AuthManager.hpp"
#include "downloader/DownloadManager.hpp"
#include "mods/ModManager.hpp"
#include "mods/ProfileManager.hpp"
#include "game/VersionManager.hpp"
#include "skin/SkinManager.hpp"
#include "../utils/PathUtils.hpp"

#include <thread>
#include <chrono>

namespace konami::core {

Application::Application() {
    Logger::instance().debug("Application instance created");
}

Application::~Application() {
    if (m_state != AppState::Uninitialized && m_state != AppState::ShuttingDown) {
        shutdown();
    }
    Logger::instance().debug("Application instance destroyed");
}

bool Application::initialize() {
    if (m_state != AppState::Uninitialized) {
        Logger::instance().warn("Application already initialized");
        return false;
    }
    
    setState(AppState::Initializing);
    Logger::instance().info("Initializing application...");
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Initialize subsystems in order
    if (!initializeAuth()) {
        Logger::instance().error("Failed to initialize authentication");
        setState(AppState::Error);
        return false;
    }
    
    if (!initializeDownloader()) {
        Logger::instance().error("Failed to initialize downloader");
        setState(AppState::Error);
        return false;
    }
    
    if (!initializeVersionManager()) {
        Logger::instance().error("Failed to initialize version manager");
        setState(AppState::Error);
        return false;
    }
    
    if (!initializeProfileManager()) {
        Logger::instance().error("Failed to initialize profile manager");
        setState(AppState::Error);
        return false;
    }
    
    if (!initializeModManager()) {
        Logger::instance().error("Failed to initialize mod manager");
        setState(AppState::Error);
        return false;
    }
    
    if (!initializeSkinManager()) {
        Logger::instance().error("Failed to initialize skin manager");
        setState(AppState::Error);
        return false;
    }
    
    if (!initializeThemeManager()) {
        Logger::instance().error("Failed to initialize theme manager");
        setState(AppState::Error);
        return false;
    }
    
    if (!initializePluginManager()) {
        Logger::instance().error("Failed to initialize plugin manager");
        setState(AppState::Error);
        return false;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    Logger::instance().info("Application initialized in {}ms", duration.count());
    
    setState(AppState::Ready);
    EventBus::instance().emit("app.initialized", {});
    
    return true;
}

void Application::shutdown() {
    if (m_state == AppState::ShuttingDown || m_state == AppState::Uninitialized) {
        return;
    }
    
    setState(AppState::ShuttingDown);
    Logger::instance().info("Shutting down application...");
    
    // Stop any running game
    stopGame();
    
    // Save configuration
    Config::instance().save(
        utils::PathUtils::getAppDataPath().string() + "/KonamiClient/config.json"
    );
    
    // Shutdown subsystems in reverse order
    m_pluginManager.reset();
    m_themeManager.reset();
    m_skinManager.reset();
    m_modManager.reset();
    m_profileManager.reset();
    m_versionManager.reset();
    m_downloadManager.reset();
    m_authManager.reset();
    
    EventBus::instance().emit("app.shutdown", {});
    Logger::instance().info("Application shutdown complete");
    
    setState(AppState::Uninitialized);
}

bool Application::isRunning() const {
    auto state = m_state.load();
    return state == AppState::Ready || state == AppState::Running || state == AppState::Launching;
}

bool Application::launchGame(const std::string& profileId) {
    if (m_state != AppState::Ready) {
        Logger::instance().warn("Cannot launch game: application not ready");
        return false;
    }
    
    setState(AppState::Launching);
    Logger::instance().info("Launching game with profile: {}", profileId);
    
    // Get profile
    auto profile = m_profileManager->getProfile(profileId);
    if (!profile) {
        Logger::instance().error("Profile not found: {}", profileId);
        setState(AppState::Ready);
        return false;
    }
    
    // Verify account is authenticated
    if (!m_authManager->isAuthenticated()) {
        Logger::instance().error("No authenticated account");
        setState(AppState::Ready);
        return false;
    }
    
    // Verify game files
    auto version = m_versionManager->getVersion(profile->versionId);
    if (!version) {
        Logger::instance().error("Version not found: {}", profile->versionId);
        setState(AppState::Ready);
        return false;
    }
    
    // Build launch arguments and start game
    // This would involve GameLauncher class
    
    setState(AppState::Running);
    EventBus::instance().emit("game.launched", {{"profileId", profileId}});
    
    return true;
}

void Application::stopGame() {
    if (m_gameProcess) {
        Logger::instance().info("Stopping game process");
        // Platform-specific process termination
#ifdef _WIN32
        // TerminateProcess(m_gameProcess, 0);
#else
        // kill(reinterpret_cast<pid_t>(m_gameProcess), SIGTERM);
#endif
        m_gameProcess = nullptr;
        setState(AppState::Ready);
        EventBus::instance().emit("game.stopped", {});
    }
}

void Application::onStateChange(std::function<void(AppState)> callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_stateCallbacks.push_back(std::move(callback));
}

void Application::setState(AppState state) {
    m_state = state;
    
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    for (const auto& callback : m_stateCallbacks) {
        try {
            callback(state);
        } catch (const std::exception& e) {
            Logger::instance().error("State callback error: {}", e.what());
        }
    }
}

bool Application::initializeAuth() {
    try {
        m_authManager = std::make_shared<auth::AuthManager>();
        m_authManager->initialize();
        
        // Try to restore previous session
        if (m_authManager->restoreSession()) {
            Logger::instance().info("Previous session restored");
        }
        
        return true;
    } catch (const std::exception& e) {
        Logger::instance().error("Auth initialization error: {}", e.what());
        return false;
    }
}

bool Application::initializeDownloader() {
    try {
        m_downloadManager = std::make_shared<downloader::DownloadManager>();
        m_downloadManager->initialize();
        return true;
    } catch (const std::exception& e) {
        Logger::instance().error("Downloader initialization error: {}", e.what());
        return false;
    }
}

bool Application::initializeModManager() {
    try {
        m_modManager = std::make_shared<mods::ModManager>();
        m_modManager->initialize();
        return true;
    } catch (const std::exception& e) {
        Logger::instance().error("Mod manager initialization error: {}", e.what());
        return false;
    }
}

bool Application::initializeProfileManager() {
    try {
        m_profileManager = std::make_shared<mods::ProfileManager>();
        m_profileManager->initialize();
        return true;
    } catch (const std::exception& e) {
        Logger::instance().error("Profile manager initialization error: {}", e.what());
        return false;
    }
}

bool Application::initializeVersionManager() {
    try {
        m_versionManager = std::make_shared<game::VersionManager>();
        m_versionManager->initialize();
        return true;
    } catch (const std::exception& e) {
        Logger::instance().error("Version manager initialization error: {}", e.what());
        return false;
    }
}

bool Application::initializeSkinManager() {
    try {
        m_skinManager = std::make_shared<skin::SkinManager>();
        m_skinManager->initialize();
        return true;
    } catch (const std::exception& e) {
        Logger::instance().error("Skin manager initialization error: {}", e.what());
        return false;
    }
}

bool Application::initializeThemeManager() {
    try {
        // ThemeManager implementation would go here
        return true;
    } catch (const std::exception& e) {
        Logger::instance().error("Theme manager initialization error: {}", e.what());
        return false;
    }
}

bool Application::initializePluginManager() {
    try {
        // PluginManager implementation would go here
        return true;
    } catch (const std::exception& e) {
        Logger::instance().error("Plugin manager initialization error: {}", e.what());
        return false;
    }
}

} // namespace konami::core
