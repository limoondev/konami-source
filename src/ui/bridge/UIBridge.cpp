// Konami Client - UI Bridge Implementation

#include "UIBridge.hpp"
#include "../../core/Application.hpp"
#include "../../core/auth/AuthManager.hpp"
#include "../../core/profile/ProfileManager.hpp"
#include "../../core/mods/ModManager.hpp"
#include "../../core/downloader/DownloadManager.hpp"
#include "../../core/launcher/GameLauncher.hpp"
#include "../../core/skin/SkinEngine.hpp"
#include "../../core/Logger.hpp"

namespace konami::ui {

UIBridge::UIBridge(Application& app)
    : m_app(app)
{
}

UIBridge::~UIBridge() = default;

bool UIBridge::initialize() {
    try {
        // Create main window
        m_window = MainWindow::create();
        
        // Setup all callbacks
        setupNavigationCallbacks();
        setupAuthCallbacks();
        setupProfileCallbacks();
        setupModCallbacks();
        setupSkinCallbacks();
        setupSettingsCallbacks();
        setupLaunchCallbacks();
        setupWindowCallbacks();
        
        // Initial data load
        updateAccountInfo();
        updateProfiles();
        updateMods();
        updateSkins();
        updateNews();
        updateSettings();
        
        m_initialized = true;
        Logger::info("UIBridge", "UI initialized successfully");
        return true;
    }
    catch (const std::exception& e) {
        Logger::error("UIBridge", "Failed to initialize UI: " + std::string(e.what()));
        return false;
    }
}

void UIBridge::run() {
    if (!m_initialized) {
        Logger::error("UIBridge", "Cannot run: UI not initialized");
        return;
    }
    
    m_window->run();
}

void UIBridge::setupNavigationCallbacks() {
    m_window->on_navigate([this](slint::SharedString page) {
        std::string pageStr = std::string(page);
        Logger::debug("UIBridge", "Navigating to: " + pageStr);
        m_window->set_current_page(page);
    });
}

void UIBridge::setupAuthCallbacks() {
    m_window->on_login([this]() {
        Logger::info("UIBridge", "Login requested");
        
        // Start Microsoft OAuth flow
        m_app.authManager().startMicrosoftAuth([this](bool success, const std::string& error) {
            if (success) {
                updateAccountInfo();
                showInfo("Login Successful", "Welcome to Konami Client!");
            } else {
                showError("Login Failed", error);
            }
        });
    });
    
    m_window->on_logout([this]() {
        Logger::info("UIBridge", "Logout requested");
        m_app.authManager().logout();
        updateAccountInfo();
    });
}

void UIBridge::setupProfileCallbacks() {
    // Profile selection
    auto& profiles_page = m_window->get_profiles_page();
    
    // Note: In actual implementation, these would be connected to the ProfilesPage component
    // This is a simplified representation
}

void UIBridge::setupModCallbacks() {
    // Mod management callbacks
    // Note: Connected to ModsPage component
}

void UIBridge::setupSkinCallbacks() {
    // Skin management callbacks
    // Note: Connected to SkinsPage component
}

void UIBridge::setupSettingsCallbacks() {
    // Settings callbacks
    auto& settings_page = m_window->get_settings_page();
    
    // Note: Connected to SettingsPage component
}

void UIBridge::setupLaunchCallbacks() {
    m_window->on_launch_game([this]() {
        Logger::info("UIBridge", "Game launch requested");
        
        // Check if logged in
        if (!m_app.authManager().isLoggedIn()) {
            showError("Cannot Launch", "Please login first to play Minecraft.");
            return;
        }
        
        // Get selected profile
        auto selectedId = std::string(m_window->get_selected_profile_id());
        if (selectedId.empty()) {
            showError("Cannot Launch", "Please select a profile first.");
            return;
        }
        
        // Start launch
        m_window->set_is_launching(true);
        m_window->set_launch_progress(0.0f);
        
        m_app.gameLauncher().launch(selectedId, 
            // Progress callback
            [this](float progress, const std::string& status) {
                slint::invoke_from_event_loop([this, progress]() {
                    m_window->set_launch_progress(progress);
                });
            },
            // Completion callback
            [this](bool success, const std::string& error) {
                slint::invoke_from_event_loop([this, success, error]() {
                    m_window->set_is_launching(false);
                    
                    if (success) {
                        updateGameStatus(true, "");
                    } else {
                        showError("Launch Failed", error);
                    }
                });
            }
        );
    });
    
    m_window->on_stop_game([this]() {
        Logger::info("UIBridge", "Game stop requested");
        m_app.gameLauncher().stop();
        updateGameStatus(false, "");
    });
}

void UIBridge::setupWindowCallbacks() {
    m_window->on_minimize_window([this]() {
        m_window->window().set_minimized(true);
    });
    
    m_window->on_maximize_window([this]() {
        auto& win = m_window->window();
        win.set_maximized(!win.is_maximized());
    });
    
    m_window->on_close_window([this]() {
        // Check if game is running
        if (m_app.gameLauncher().isRunning()) {
            showConfirm("Confirm Exit", 
                "Minecraft is still running. Are you sure you want to close?",
                [this](bool confirmed) {
                    if (confirmed) {
                        m_window->hide();
                    }
                });
        } else {
            m_window->hide();
        }
    });
}

void UIBridge::updateAccountInfo() {
    if (!m_app.authManager().isLoggedIn()) {
        AccountInfo info;
        info.is_logged_in = false;
        info.username = slint::SharedString("Not logged in");
        info.uuid = slint::SharedString("");
        info.avatar_url = slint::SharedString("");
        info.account_type = slint::SharedString("");
        m_window->set_account(info);
        return;
    }
    
    auto& account = m_app.authManager().currentAccount();
    AccountInfo info;
    info.is_logged_in = true;
    info.username = slint::SharedString(account.username);
    info.uuid = slint::SharedString(account.uuid);
    info.avatar_url = slint::SharedString(account.avatarUrl);
    info.account_type = slint::SharedString(account.type == AccountType::Microsoft ? "microsoft" : "offline");
    
    m_window->set_account(info);
}

void UIBridge::updateProfiles() {
    auto& profileManager = m_app.profileManager();
    auto profiles = profileManager.getAllProfiles();
    
    auto model = std::make_shared<slint::VectorModel<ProfileInfo>>();
    
    for (const auto& profile : profiles) {
        ProfileInfo info;
        info.id = slint::SharedString(profile.id);
        info.name = slint::SharedString(profile.name);
        info.game_version = slint::SharedString(profile.gameVersion);
        info.loader = slint::SharedString(profile.loader);
        info.loader_version = slint::SharedString(profile.loaderVersion);
        info.icon = slint::SharedString(profile.icon);
        info.last_played = slint::SharedString(profile.lastPlayed);
        info.total_playtime = slint::SharedString(profile.totalPlaytime);
        info.mod_count = profile.modCount;
        info.is_favorite = profile.isFavorite;
        info.created_at = slint::SharedString(profile.createdAt);
        
        model->push_back(info);
    }
    
    m_window->set_profiles(model);
}

void UIBridge::updateMods() {
    auto& modManager = m_app.modManager();
    
    // Installed mods
    auto installedMods = modManager.getInstalledMods();
    auto installedModel = std::make_shared<slint::VectorModel<ModInfo>>();
    
    for (const auto& mod : installedMods) {
        ModInfo info;
        info.id = slint::SharedString(mod.id);
        info.name = slint::SharedString(mod.name);
        info.author = slint::SharedString(mod.author);
        info.description = slint::SharedString(mod.description);
        info.version = slint::SharedString(mod.version);
        info.game_version = slint::SharedString(mod.gameVersion);
        info.downloads = mod.downloads;
        info.icon_url = slint::SharedString(mod.iconUrl);
        info.is_installed = true;
        info.is_enabled = mod.isEnabled;
        info.is_updating = false;
        info.source = slint::SharedString(mod.source);
        info.category = slint::SharedString(mod.category);
        
        installedModel->push_back(info);
    }
    
    m_window->set_installed_mods(installedModel);
}

void UIBridge::updateSkins() {
    auto& skinEngine = m_app.skinEngine();
    auto skins = skinEngine.getAllSkins();
    
    auto model = std::make_shared<slint::VectorModel<SkinInfo>>();
    
    for (const auto& skin : skins) {
        SkinInfo info;
        info.id = slint::SharedString(skin.id);
        info.name = slint::SharedString(skin.name);
        info.texture_url = slint::SharedString(skin.textureUrl);
        info.model_type = slint::SharedString(skin.modelType);
        info.is_active = skin.isActive;
        info.is_favorite = skin.isFavorite;
        info.created_at = slint::SharedString(skin.createdAt);
        
        model->push_back(info);
    }
    
    m_window->set_skins(model);
    
    // Also update capes
    auto capes = skinEngine.getAllCapes();
    auto capesModel = std::make_shared<slint::VectorModel<CapeInfo>>();
    
    for (const auto& cape : capes) {
        CapeInfo info;
        info.id = slint::SharedString(cape.id);
        info.name = slint::SharedString(cape.name);
        info.texture_url = slint::SharedString(cape.textureUrl);
        info.is_active = cape.isActive;
        info.source = slint::SharedString(cape.source);
        
        capesModel->push_back(info);
    }
    
    m_window->set_capes(capesModel);
}

void UIBridge::updateDownloadProgress(float progress, const std::string& currentFile) {
    DownloadProgress dp;
    dp.is_downloading = progress < 1.0f;
    dp.current_file = slint::SharedString(currentFile);
    dp.current_progress = progress;
    dp.total_progress = progress;
    dp.download_speed = slint::SharedString("0 MB/s"); // Would calculate actual speed
    dp.eta = slint::SharedString("--:--");
    dp.files_completed = 0;
    dp.files_total = 1;
    
    m_window->set_download_progress(dp);
}

void UIBridge::updateGameStatus(bool running, const std::string& memoryUsage) {
    GameStatus status;
    status.is_running = running;
    status.current_version = m_window->get_current_version();
    status.memory_usage = slint::SharedString(memoryUsage);
    status.uptime = slint::SharedString("00:00:00");
    
    m_window->set_game_status(status);
}

void UIBridge::updateNews() {
    // Would fetch from news API
    auto model = std::make_shared<slint::VectorModel<NewsItem>>();
    
    // Example news items
    NewsItem item1;
    item1.id = slint::SharedString("1");
    item1.title = slint::SharedString("Minecraft 1.21.4 Released!");
    item1.summary = slint::SharedString("The latest update brings exciting new features...");
    item1.image_url = slint::SharedString("");
    item1.date = slint::SharedString("Today");
    item1.url = slint::SharedString("https://minecraft.net");
    item1.category = slint::SharedString("Update");
    model->push_back(item1);
    
    NewsItem item2;
    item2.id = slint::SharedString("2");
    item2.title = slint::SharedString("Konami Client v1.0 Launch");
    item2.summary = slint::SharedString("Welcome to the revolutionary Minecraft launcher!");
    item2.image_url = slint::SharedString("");
    item2.date = slint::SharedString("Yesterday");
    item2.url = slint::SharedString("");
    item2.category = slint::SharedString("Launcher");
    model->push_back(item2);
    
    m_window->set_news(model);
}

void UIBridge::updateSettings() {
    auto& config = m_app.config();
    
    LauncherSettings settings;
    settings.language = slint::SharedString(config.getString("general.language", "English"));
    settings.auto_update = config.getBool("general.autoUpdate", true);
    settings.minimize_on_launch = config.getBool("general.minimizeOnLaunch", true);
    settings.close_on_launch = config.getBool("general.closeOnLaunch", false);
    settings.show_news = config.getBool("general.showNews", true);
    
    settings.theme_name = slint::SharedString(config.getString("appearance.theme", "Konami Dark"));
    settings.use_blur_effects = config.getBool("appearance.useBlurEffects", true);
    settings.animation_speed = config.getFloat("appearance.animationSpeed", 1.0f);
    
    settings.java_path = slint::SharedString(config.getString("java.path", "auto"));
    settings.min_memory = config.getInt("java.minMemory", 1024);
    settings.max_memory = config.getInt("java.maxMemory", 4096);
    settings.jvm_args = slint::SharedString(config.getString("java.jvmArgs", ""));
    
    settings.concurrent_downloads = config.getInt("performance.concurrentDownloads", 4);
    settings.use_cache = config.getBool("performance.useCache", true);
    
    settings.game_directory = slint::SharedString(config.getString("advanced.gameDirectory", ""));
    settings.keep_launcher_open = config.getBool("advanced.keepLauncherOpen", true);
    settings.show_console = config.getBool("advanced.showConsole", false);
    
    m_window->set_settings(settings);
}

void UIBridge::showError(const std::string& title, const std::string& message) {
    // Would show error dialog
    Logger::error("UIBridge", title + ": " + message);
}

void UIBridge::showInfo(const std::string& title, const std::string& message) {
    // Would show info dialog
    Logger::info("UIBridge", title + ": " + message);
}

void UIBridge::showConfirm(const std::string& title, const std::string& message,
                           std::function<void(bool)> callback) {
    // Would show confirmation dialog
    Logger::info("UIBridge", "Confirm: " + title + ": " + message);
    // For now, auto-confirm
    callback(true);
}

void UIBridge::navigateTo(const std::string& page) {
    m_window->set_current_page(slint::SharedString(page));
}

} // namespace konami::ui
