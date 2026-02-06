/**
 * Konami Client - Revolutionary Minecraft Launcher
 * 
 * Main entry point for the application.
 * Initializes the Slint UI framework and core application components.
 * 
 * @author Konami Team
 * @version 1.0.0
 * @license MIT
 */

#include <slint.h>
#include <memory>
#include <iostream>
#include <csignal>
#include <cstdlib>
#include <filesystem>

#include "core/Application.hpp"
#include "core/Logger.hpp"
#include "core/Config.hpp"
#include "core/EventBus.hpp"

namespace fs = std::filesystem;

// Global application instance for signal handling
std::unique_ptr<konami::core::Application> g_app;

/**
 * Signal handler for graceful shutdown
 */
void signalHandler(int signal) {
    konami::core::Logger::instance().info("Received signal {}, shutting down gracefully...", signal);
    if (g_app) {
        g_app->shutdown();
    }
}

/**
 * Setup signal handlers for graceful shutdown
 */
void setupSignalHandlers() {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
#ifdef _WIN32
    std::signal(SIGBREAK, signalHandler);
#endif
}

/**
 * Initialize application directories
 */
bool initializeDirectories() {
    auto& logger = konami::core::Logger::instance();
    
    try {
        // Get base paths (platform-specific app data path)
        fs::path appDataPath;
#ifdef _WIN32
        const char* appData = std::getenv("APPDATA");
        appDataPath = appData ? fs::path(appData) : fs::current_path();
#elif defined(__APPLE__)
        const char* home = std::getenv("HOME");
        appDataPath = home ? fs::path(home) / "Library" / "Application Support" : fs::current_path();
#else
        const char* home = std::getenv("HOME");
        appDataPath = home ? fs::path(home) / ".local" / "share" : fs::current_path();
#endif
        fs::path launcherPath = appDataPath / "KonamiClient";
        
        // Create directory structure
        std::vector<fs::path> directories = {
            launcherPath,
            launcherPath / "instances",
            launcherPath / "mods",
            launcherPath / "skins",
            launcherPath / "cache",
            launcherPath / "logs",
            launcherPath / "themes",
            launcherPath / "plugins",
            launcherPath / "assets",
            launcherPath / "libraries",
            launcherPath / "versions",
            launcherPath / "profiles",
            launcherPath / "backups"
        };
        
        for (const auto& dir : directories) {
            if (!fs::exists(dir)) {
                fs::create_directories(dir);
                logger.debug("Created directory: {}", dir.string());
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        logger.error("Failed to initialize directories: {}", e.what());
        return false;
    }
}

/**
 * Load and apply configuration
 */
bool loadConfiguration() {
    auto& logger = konami::core::Logger::instance();
    auto& config = konami::core::Config::instance();
    
    try {
        // Determine config path (same logic as directories)
        fs::path appDataPath;
#ifdef _WIN32
        const char* appData = std::getenv("APPDATA");
        appDataPath = appData ? fs::path(appData) : fs::current_path();
#elif defined(__APPLE__)
        const char* home = std::getenv("HOME");
        appDataPath = home ? fs::path(home) / "Library" / "Application Support" : fs::current_path();
#else
        const char* home = std::getenv("HOME");
        appDataPath = home ? fs::path(home) / ".local" / "share" : fs::current_path();
#endif
        fs::path configPath = appDataPath / "KonamiClient" / "config.json";
        
        if (fs::exists(configPath)) {
            config.load(configPath.string());
            logger.info("Configuration loaded from {}", configPath.string());
        } else {
            config.setDefaults();
            config.save(configPath.string());
            logger.info("Default configuration created at {}", configPath.string());
        }
        
        return true;
    } catch (const std::exception& e) {
        logger.error("Failed to load configuration: {}", e.what());
        return false;
    }
}

/**
 * Initialize the Slint UI
 */
std::shared_ptr<MainWindow> initializeUI() {
    auto& logger = konami::core::Logger::instance();
    
    try {
        // Create main window
        auto mainWindow = MainWindow::create();
        
        // Set window properties
        mainWindow->window().set_title("KonamiClient");
        
        // Set minimum size
        slint::WindowSize minSize{1024, 720};
        // mainWindow->window().set_minimum_size(minSize);
        
        logger.info("Slint UI initialized successfully");
        return mainWindow;
        
    } catch (const std::exception& e) {
        logger.error("Failed to initialize Slint UI: {}", e.what());
        return nullptr;
    }
}

/**
 * Main application entry point
 */
int main(int argc, char* argv[]) {
    // Parse command line arguments
    bool debugMode = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--debug" || arg == "-d") {
            debugMode = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "KonamiClient - Revolutionary Minecraft Launcher\n"
                      << "\nUsage: " << argv[0] << " [options]\n"
                      << "\nOptions:\n"
                      << "  -d, --debug    Enable debug mode\n"
                      << "  -h, --help     Show this help message\n"
                      << "  -v, --version  Show version information\n"
                      << std::endl;
            return 0;
        } else if (arg == "--version" || arg == "-v") {
            std::cout << "KonamiClient v1.0.0\n"
                      << "Built with Slint UI Framework\n"
                      << "Copyright (c) 2024 Konami Team\n"
                      << std::endl;
            return 0;
        }
    }
    
    // Initialize logger
    konami::core::Logger::instance().initialize(
        debugMode ? konami::core::LogLevel::Debug : konami::core::LogLevel::Info
    );
    
    auto& logger = konami::core::Logger::instance();
    logger.info("KonamiClient v1.0.0 starting...");
#ifdef _WIN32
    logger.info("Platform: Windows");
#elif defined(__APPLE__)
    logger.info("Platform: macOS");
#else
    logger.info("Platform: Linux");
#endif
    
    // Setup signal handlers
    setupSignalHandlers();
    
    // Initialize directories
    if (!initializeDirectories()) {
        logger.critical("Failed to initialize application directories");
        return 1;
    }
    
    // Load configuration
    if (!loadConfiguration()) {
        logger.critical("Failed to load configuration");
        return 1;
    }
    
    // Initialize application
    try {
        g_app = std::make_unique<konami::core::Application>();
        
        if (!g_app->initialize()) {
            logger.critical("Failed to initialize application");
            return 1;
        }
        
        // Initialize Slint UI
        auto mainWindow = initializeUI();
        if (!mainWindow) {
            logger.critical("Failed to initialize UI");
            return 1;
        }
        
        // Subscribe to application events
        konami::core::EventBus::instance().subscribe("app.exit", [&mainWindow](const auto&) {
            mainWindow->window().hide();
        });
        
        logger.info("Application initialized successfully");
        
        // Show window and run event loop
        mainWindow->run();
        
        // Cleanup
        g_app->shutdown();
        
        logger.info("KonamiClient shutdown complete");
        return 0;
        
    } catch (const std::exception& e) {
        logger.critical("Unhandled exception: {}", e.what());
        return 1;
    }
}
