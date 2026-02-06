#include "GameLauncher.hpp"
#include "../Logger.hpp"
#include "../downloader/DownloadManager.hpp"
#include <fstream>
#include <sstream>
#include <regex>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#else
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#endif

namespace konami::launcher {

class GameLauncher::Impl {
public:
    std::filesystem::path gameDirectory;
    std::filesystem::path assetsDirectory;
    std::filesystem::path librariesDirectory;
    std::filesystem::path versionsDirectory;
    std::filesystem::path nativesDirectory;
    
    VersionManifest versionManifest;
    std::vector<VersionInfo> installedVersions;
    
    LaunchState currentState = LaunchState::Idle;
    ProcessInfo currentProcess;
    std::vector<std::string> gameLog;
    
    OutputCallback outputCallback;
    StateCallback stateCallback;
    std::function<void()> onGameStarted;
    std::function<void(int)> onGameExited;
    std::function<void(const std::string&)> onGameCrashed;
    
    std::mutex logMutex;
    std::mutex stateMutex;
    
#ifdef _WIN32
    HANDLE processHandle = nullptr;
    HANDLE outputThread = nullptr;
#else
    pid_t processPid = 0;
    std::thread outputThread;
#endif
    
    bool running = false;
    
    void setState(LaunchState state) {
        std::lock_guard<std::mutex> lock(stateMutex);
        currentState = state;
        if (stateCallback) {
            stateCallback(state);
        }
    }
    
    void addLogLine(const std::string& line, bool isError) {
        std::lock_guard<std::mutex> lock(logMutex);
        gameLog.push_back(line);
        
        if (outputCallback) {
            outputCallback(line, isError);
        }
    }
    
    nlohmann::json loadVersionJson(const std::string& version) {
        auto versionJsonPath = versionsDirectory / version / (version + ".json");
        
        if (!std::filesystem::exists(versionJsonPath)) {
            return nullptr;
        }
        
        std::ifstream file(versionJsonPath);
        if (!file) return nullptr;
        
        nlohmann::json j;
        file >> j;
        return j;
    }
    
    std::vector<LibraryInfo> parseLibraries(const nlohmann::json& versionJson) {
        std::vector<LibraryInfo> libraries;
        
        if (!versionJson.contains("libraries")) return libraries;
        
        for (const auto& lib : versionJson["libraries"]) {
            LibraryInfo info;
            info.name = lib.value("name", "");
            
            // Parse name to path
            auto parts = splitString(info.name, ':');
            if (parts.size() >= 3) {
                std::string group = parts[0];
                std::replace(group.begin(), group.end(), '.', '/');
                info.path = group + "/" + parts[1] + "/" + parts[2] + "/" + 
                           parts[1] + "-" + parts[2] + ".jar";
            }
            
            // Download info
            if (lib.contains("downloads") && lib["downloads"].contains("artifact")) {
                auto& artifact = lib["downloads"]["artifact"];
                info.url = artifact.value("url", "");
                info.sha1 = artifact.value("sha1", "");
                info.size = artifact.value("size", 0);
                if (artifact.contains("path")) {
                    info.path = artifact["path"].get<std::string>();
                }
            }
            
            // Check for natives
            if (lib.contains("natives")) {
#ifdef _WIN32
                if (lib["natives"].contains("windows")) {
                    info.native = true;
                    info.nativeClassifier = lib["natives"]["windows"].get<std::string>();
                }
#elif __APPLE__
                if (lib["natives"].contains("osx")) {
                    info.native = true;
                    info.nativeClassifier = lib["natives"]["osx"].get<std::string>();
                }
#else
                if (lib["natives"].contains("linux")) {
                    info.native = true;
                    info.nativeClassifier = lib["natives"]["linux"].get<std::string>();
                }
#endif
            }
            
            // Rules
            bool allowed = true;
            if (lib.contains("rules")) {
                allowed = false;
                for (const auto& rule : lib["rules"]) {
                    std::string action = rule.value("action", "allow");
                    bool matches = true;
                    
                    if (rule.contains("os")) {
                        std::string osName = rule["os"].value("name", "");
#ifdef _WIN32
                        matches = (osName == "windows" || osName.empty());
#elif __APPLE__
                        matches = (osName == "osx" || osName.empty());
#else
                        matches = (osName == "linux" || osName.empty());
#endif
                    }
                    
                    if (matches) {
                        allowed = (action == "allow");
                    }
                }
            }
            
            if (allowed) {
                libraries.push_back(info);
            }
        }
        
        return libraries;
    }
    
    std::vector<std::string> splitString(const std::string& str, char delimiter) {
        std::vector<std::string> parts;
        std::stringstream ss(str);
        std::string part;
        while (std::getline(ss, part, delimiter)) {
            parts.push_back(part);
        }
        return parts;
    }
};

GameLauncher::GameLauncher() : m_impl(std::make_unique<Impl>()) {}

GameLauncher::~GameLauncher() {
    shutdown();
}

bool GameLauncher::initialize(const std::filesystem::path& gameDirectory) {
    m_impl->gameDirectory = gameDirectory;
    m_impl->assetsDirectory = gameDirectory / "assets";
    m_impl->librariesDirectory = gameDirectory / "libraries";
    m_impl->versionsDirectory = gameDirectory / "versions";
    m_impl->nativesDirectory = gameDirectory / "natives";
    
    // Create directories
    std::filesystem::create_directories(m_impl->assetsDirectory);
    std::filesystem::create_directories(m_impl->librariesDirectory);
    std::filesystem::create_directories(m_impl->versionsDirectory);
    std::filesystem::create_directories(m_impl->nativesDirectory);
    
    // Scan installed versions
    for (const auto& entry : std::filesystem::directory_iterator(m_impl->versionsDirectory)) {
        if (entry.is_directory()) {
            auto versionId = entry.path().filename().string();
            auto versionJson = entry.path() / (versionId + ".json");
            
            if (std::filesystem::exists(versionJson)) {
                VersionInfo info;
                info.id = versionId;
                m_impl->installedVersions.push_back(info);
            }
        }
    }
    
    core::Logger::info("GameLauncher", "Initialized with {} installed versions", 
        m_impl->installedVersions.size());
    return true;
}

void GameLauncher::shutdown() {
    if (isRunning()) {
        kill();
    }
}

std::future<VersionManifest> GameLauncher::fetchVersionManifest() {
    return std::async(std::launch::async, [this]() {
        VersionManifest manifest;
        
        // HTTP request to Mojang API would go here
        const std::string manifestUrl = "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json";
        
        // Placeholder - would use CURL/cpr for actual HTTP request
        core::Logger::info("GameLauncher", "Fetching version manifest from Mojang");
        
        m_impl->versionManifest = manifest;
        return manifest;
    });
}

std::vector<VersionInfo> GameLauncher::getAvailableVersions() const {
    return m_impl->versionManifest.versions;
}

std::vector<VersionInfo> GameLauncher::getInstalledVersions() const {
    return m_impl->installedVersions;
}

bool GameLauncher::isVersionInstalled(const std::string& version) const {
    auto it = std::find_if(m_impl->installedVersions.begin(), m_impl->installedVersions.end(),
        [&version](const VersionInfo& v) { return v.id == version; });
    return it != m_impl->installedVersions.end();
}

std::future<bool> GameLauncher::launch(const LaunchOptions& options, ProgressCallback progressCallback) {
    return std::async(std::launch::async, [this, options, progressCallback]() {
        core::Logger::info("GameLauncher", "Launching profile: {}", options.profileId);
        
        m_impl->setState(LaunchState::Preparing);
        
        if (progressCallback) {
            LaunchProgress progress;
            progress.state = LaunchState::Preparing;
            progress.message = "Preparing launch...";
            progress.progress = 0.0;
            progressCallback(progress);
        }
        
        // Load profile - would get from ProfileManager
        profile::Profile profile;
        profile.id = options.profileId;
        profile.gameVersion = "1.20.4"; // Example
        
        auto versionJson = m_impl->loadVersionJson(profile.gameVersion);
        if (versionJson.is_null()) {
            core::Logger::error("GameLauncher", "Version JSON not found for: {}", profile.gameVersion);
            return false;
        }
        
        // Download assets
        m_impl->setState(LaunchState::DownloadingAssets);
        if (progressCallback) {
            LaunchProgress progress;
            progress.state = LaunchState::DownloadingAssets;
            progress.message = "Checking assets...";
            progress.progress = 0.1;
            progressCallback(progress);
        }
        
        // Download libraries
        m_impl->setState(LaunchState::DownloadingLibraries);
        if (progressCallback) {
            LaunchProgress progress;
            progress.state = LaunchState::DownloadingLibraries;
            progress.message = "Checking libraries...";
            progress.progress = 0.3;
            progressCallback(progress);
        }
        
        // Build command line
        m_impl->setState(LaunchState::Building);
        
        auto jvmArgs = buildJvmArguments(profile, options);
        auto gameArgs = buildGameArguments(profile, options);
        auto classpath = buildClasspath(profile.gameVersion);
        
        // Extract natives
        extractNatives(profile.gameVersion);
        
        // Build full command
        std::vector<std::string> command;
        command.push_back(profile.javaConfig.path.empty() ? "java" : profile.javaConfig.path);
        
        for (const auto& arg : jvmArgs) {
            command.push_back(arg);
        }
        
        command.push_back("-cp");
        command.push_back(classpath);
        
        std::string mainClass = versionJson.value("mainClass", "net.minecraft.client.main.Main");
        command.push_back(mainClass);
        
        for (const auto& arg : gameArgs) {
            command.push_back(arg);
        }
        
        // Launch
        m_impl->setState(LaunchState::Launching);
        if (progressCallback) {
            LaunchProgress progress;
            progress.state = LaunchState::Launching;
            progress.message = "Launching Minecraft...";
            progress.progress = 0.9;
            progressCallback(progress);
        }
        
        core::Logger::info("GameLauncher", "Starting game process");
        
#ifdef _WIN32
        // Windows process creation
        std::string cmdLine;
        for (const auto& arg : command) {
            if (!cmdLine.empty()) cmdLine += " ";
            if (arg.find(' ') != std::string::npos) {
                cmdLine += "\"" + arg + "\"";
            } else {
                cmdLine += arg;
            }
        }
        
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        
        si.dwFlags = STARTF_USESTDHANDLES;
        
        SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
        HANDLE stdoutRead, stdoutWrite;
        CreatePipe(&stdoutRead, &stdoutWrite, &sa, 0);
        si.hStdOutput = stdoutWrite;
        si.hStdError = stdoutWrite;
        
        if (CreateProcessA(nullptr, const_cast<char*>(cmdLine.c_str()),
            nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
            profile.gameDirectory.c_str(), &si, &pi)) {
            
            m_impl->processHandle = pi.hProcess;
            m_impl->currentProcess.pid = pi.dwProcessId;
            m_impl->currentProcess.startTime = std::chrono::system_clock::now();
            m_impl->running = true;
            
            CloseHandle(pi.hThread);
            CloseHandle(stdoutWrite);
            
            // Read output in separate thread
            m_impl->outputThread = CreateThread(nullptr, 0, 
                [](LPVOID param) -> DWORD {
                    // Output reading logic
                    return 0;
                }, stdoutRead, 0, nullptr);
        }
#else
        // Unix process creation
        m_impl->processPid = fork();
        
        if (m_impl->processPid == 0) {
            // Child process
            std::vector<char*> args;
            for (auto& arg : command) {
                args.push_back(const_cast<char*>(arg.c_str()));
            }
            args.push_back(nullptr);
            
            chdir(profile.gameDirectory.c_str());
            execvp(args[0], args.data());
            exit(1);
        } else if (m_impl->processPid > 0) {
            m_impl->currentProcess.pid = m_impl->processPid;
            m_impl->currentProcess.startTime = std::chrono::system_clock::now();
            m_impl->running = true;
            
            // Start output reading thread
            m_impl->outputThread = std::thread([this]() {
                int status;
                waitpid(m_impl->processPid, &status, 0);
                
                m_impl->running = false;
                m_impl->currentProcess.endTime = std::chrono::system_clock::now();
                m_impl->currentProcess.exitCode = WEXITSTATUS(status);
                
                m_impl->setState(LaunchState::Finished);
                
                if (m_impl->onGameExited) {
                    m_impl->onGameExited(m_impl->currentProcess.exitCode);
                }
            });
            m_impl->outputThread.detach();
        }
#endif
        
        m_impl->setState(LaunchState::Running);
        
        if (m_impl->onGameStarted) {
            m_impl->onGameStarted();
        }
        
        if (progressCallback) {
            LaunchProgress progress;
            progress.state = LaunchState::Running;
            progress.message = "Game running";
            progress.progress = 1.0;
            progressCallback(progress);
        }
        
        return true;
    });
}

std::future<bool> GameLauncher::launchProfile(const std::string& profileId, ProgressCallback progressCallback) {
    LaunchOptions options;
    options.profileId = profileId;
    return launch(options, progressCallback);
}

bool GameLauncher::isRunning() const {
    return m_impl->running;
}

ProcessInfo GameLauncher::getProcessInfo() const {
    return m_impl->currentProcess;
}

void GameLauncher::kill() {
    if (!isRunning()) return;
    
#ifdef _WIN32
    TerminateProcess(m_impl->processHandle, 1);
    CloseHandle(m_impl->processHandle);
    m_impl->processHandle = nullptr;
#else
    ::kill(m_impl->processPid, SIGTERM);
#endif
    
    m_impl->running = false;
    m_impl->setState(LaunchState::Finished);
}

void GameLauncher::forceKill() {
    if (!isRunning()) return;
    
#ifdef _WIN32
    TerminateProcess(m_impl->processHandle, 9);
#else
    ::kill(m_impl->processPid, SIGKILL);
#endif
    
    m_impl->running = false;
    m_impl->setState(LaunchState::Finished);
}

void GameLauncher::setOutputCallback(OutputCallback callback) {
    m_impl->outputCallback = std::move(callback);
}

std::vector<std::string> GameLauncher::getGameLog() const {
    std::lock_guard<std::mutex> lock(m_impl->logMutex);
    return m_impl->gameLog;
}

void GameLauncher::clearGameLog() {
    std::lock_guard<std::mutex> lock(m_impl->logMutex);
    m_impl->gameLog.clear();
}

LaunchState GameLauncher::getState() const {
    return m_impl->currentState;
}

void GameLauncher::setStateCallback(StateCallback callback) {
    m_impl->stateCallback = std::move(callback);
}

void GameLauncher::setGameDirectory(const std::filesystem::path& path) {
    m_impl->gameDirectory = path;
}

std::filesystem::path GameLauncher::getGameDirectory() const {
    return m_impl->gameDirectory;
}

void GameLauncher::setAssetsDirectory(const std::filesystem::path& path) {
    m_impl->assetsDirectory = path;
}

void GameLauncher::setLibrariesDirectory(const std::filesystem::path& path) {
    m_impl->librariesDirectory = path;
}

void GameLauncher::setOnGameStarted(std::function<void()> callback) {
    m_impl->onGameStarted = std::move(callback);
}

void GameLauncher::setOnGameExited(std::function<void(int)> callback) {
    m_impl->onGameExited = std::move(callback);
}

void GameLauncher::setOnGameCrashed(std::function<void(const std::string&)> callback) {
    m_impl->onGameCrashed = std::move(callback);
}

std::vector<std::string> GameLauncher::buildJvmArguments(const profile::Profile& profile, const LaunchOptions& options) {
    std::vector<std::string> args;
    
    // Memory
    args.push_back("-Xms" + std::to_string(profile.javaConfig.minMemoryMB) + "M");
    args.push_back("-Xmx" + std::to_string(profile.javaConfig.maxMemoryMB) + "M");
    
    // GC settings
    args.push_back("-XX:+UseG1GC");
    args.push_back("-XX:+ParallelRefProcEnabled");
    args.push_back("-XX:MaxGCPauseMillis=200");
    args.push_back("-XX:+UnlockExperimentalVMOptions");
    args.push_back("-XX:+DisableExplicitGC");
    args.push_back("-XX:+AlwaysPreTouch");
    args.push_back("-XX:G1NewSizePercent=30");
    args.push_back("-XX:G1MaxNewSizePercent=40");
    args.push_back("-XX:G1HeapRegionSize=8M");
    args.push_back("-XX:G1ReservePercent=20");
    args.push_back("-XX:G1HeapWastePercent=5");
    args.push_back("-XX:G1MixedGCCountTarget=4");
    args.push_back("-XX:InitiatingHeapOccupancyPercent=15");
    args.push_back("-XX:G1MixedGCLiveThresholdPercent=90");
    args.push_back("-XX:G1RSetUpdatingPauseTimePercent=5");
    args.push_back("-XX:SurvivorRatio=32");
    args.push_back("-XX:+PerfDisableSharedMem");
    args.push_back("-XX:MaxTenuringThreshold=1");
    
    // Natives path
    auto nativesPath = m_impl->nativesDirectory / profile.gameVersion;
    args.push_back("-Djava.library.path=" + nativesPath.string());
    
    // Launcher name
    args.push_back("-Dminecraft.launcher.brand=KonamiClient");
    args.push_back("-Dminecraft.launcher.version=1.0.0");
    
    // Custom JVM args
    for (const auto& arg : profile.javaConfig.jvmArgs) {
        args.push_back(arg);
    }
    
    // Extra JVM args from options
    for (const auto& arg : options.extraJvmArgs) {
        args.push_back(arg);
    }
    
    return args;
}

std::vector<std::string> GameLauncher::buildGameArguments(const profile::Profile& profile, const LaunchOptions& options) {
    std::vector<std::string> args;
    
    // Required arguments
    args.push_back("--username");
    args.push_back(options.offlineMode ? options.offlineUsername : "Player");
    
    args.push_back("--version");
    args.push_back(profile.gameVersion);
    
    args.push_back("--gameDir");
    args.push_back(profile.gameDirectory);
    
    args.push_back("--assetsDir");
    args.push_back(m_impl->assetsDirectory.string());
    
    args.push_back("--assetIndex");
    args.push_back(profile.gameVersion);
    
    // UUID and access token would come from auth
    args.push_back("--uuid");
    args.push_back("00000000-0000-0000-0000-000000000000");
    
    args.push_back("--accessToken");
    args.push_back(options.offlineMode ? "0" : "");
    
    args.push_back("--userType");
    args.push_back(options.offlineMode ? "legacy" : "msa");
    
    // Resolution
    if (!profile.resolution.fullscreen) {
        args.push_back("--width");
        args.push_back(std::to_string(profile.resolution.width));
        args.push_back("--height");
        args.push_back(std::to_string(profile.resolution.height));
    } else {
        args.push_back("--fullscreen");
    }
    
    // Quick play
    if (options.quickPlay) {
        if (!options.quickPlayServer.empty()) {
            args.push_back("--quickPlayMultiplayer");
            args.push_back(options.quickPlayServer);
        } else if (!options.quickPlayWorld.empty()) {
            args.push_back("--quickPlaySingleplayer");
            args.push_back(options.quickPlayWorld);
        }
    }
    
    // Demo mode
    if (options.demoMode) {
        args.push_back("--demo");
    }
    
    // Extra game args
    for (const auto& arg : options.extraGameArgs) {
        args.push_back(arg);
    }
    
    return args;
}

std::string GameLauncher::buildClasspath(const std::string& version) {
    std::vector<std::string> classpathEntries;
    
    auto versionJson = m_impl->loadVersionJson(version);
    if (versionJson.is_null()) return "";
    
    auto libraries = m_impl->parseLibraries(versionJson);
    
    for (const auto& lib : libraries) {
        if (!lib.native) {
            auto libPath = m_impl->librariesDirectory / lib.path;
            if (std::filesystem::exists(libPath)) {
                classpathEntries.push_back(libPath.string());
            }
        }
    }
    
    // Add client jar
    auto clientJar = m_impl->versionsDirectory / version / (version + ".jar");
    if (std::filesystem::exists(clientJar)) {
        classpathEntries.push_back(clientJar.string());
    }
    
#ifdef _WIN32
    const char separator = ';';
#else
    const char separator = ':';
#endif
    
    std::string classpath;
    for (size_t i = 0; i < classpathEntries.size(); ++i) {
        if (i > 0) classpath += separator;
        classpath += classpathEntries[i];
    }
    
    return classpath;
}

bool GameLauncher::extractNatives(const std::string& version) {
    auto nativesDir = m_impl->nativesDirectory / version;
    std::filesystem::create_directories(nativesDir);
    
    auto versionJson = m_impl->loadVersionJson(version);
    if (versionJson.is_null()) return false;
    
    auto libraries = m_impl->parseLibraries(versionJson);
    
    for (const auto& lib : libraries) {
        if (lib.native) {
            // Extract native library - would use libzip
            core::Logger::debug("GameLauncher", "Extracting native: {}", lib.name);
        }
    }
    
    return true;
}

// JvmArgumentBuilder implementation
JvmArgumentBuilder& JvmArgumentBuilder::withMemory(int minMB, int maxMB) {
    m_minMemory = minMB;
    m_maxMemory = maxMB;
    return *this;
}

JvmArgumentBuilder& JvmArgumentBuilder::withGC(const std::string& gcType) {
    m_gcType = gcType;
    return *this;
}

JvmArgumentBuilder& JvmArgumentBuilder::withNatives(const std::filesystem::path& nativesPath) {
    m_nativesPath = nativesPath;
    return *this;
}

JvmArgumentBuilder& JvmArgumentBuilder::withClasspath(const std::string& classpath) {
    m_classpath = classpath;
    return *this;
}

JvmArgumentBuilder& JvmArgumentBuilder::withMainClass(const std::string& mainClass) {
    m_mainClass = mainClass;
    return *this;
}

JvmArgumentBuilder& JvmArgumentBuilder::withProperty(const std::string& key, const std::string& value) {
    m_properties.emplace_back(key, value);
    return *this;
}

JvmArgumentBuilder& JvmArgumentBuilder::withCustomArg(const std::string& arg) {
    m_customArgs.push_back(arg);
    return *this;
}

JvmArgumentBuilder& JvmArgumentBuilder::withGCLogging(bool enabled) {
    m_gcLogging = enabled;
    return *this;
}

JvmArgumentBuilder& JvmArgumentBuilder::withLargePages(bool enabled) {
    m_largePages = enabled;
    return *this;
}

std::vector<std::string> JvmArgumentBuilder::build() const {
    std::vector<std::string> args;
    
    args.push_back("-Xms" + std::to_string(m_minMemory) + "M");
    args.push_back("-Xmx" + std::to_string(m_maxMemory) + "M");
    
    args.push_back("-XX:+Use" + m_gcType);
    
    if (!m_nativesPath.empty()) {
        args.push_back("-Djava.library.path=" + m_nativesPath.string());
    }
    
    for (const auto& [key, value] : m_properties) {
        args.push_back("-D" + key + "=" + value);
    }
    
    if (m_gcLogging) {
        args.push_back("-Xlog:gc*:file=gc.log");
    }
    
    if (m_largePages) {
        args.push_back("-XX:+UseLargePages");
    }
    
    for (const auto& arg : m_customArgs) {
        args.push_back(arg);
    }
    
    if (!m_classpath.empty()) {
        args.push_back("-cp");
        args.push_back(m_classpath);
    }
    
    if (!m_mainClass.empty()) {
        args.push_back(m_mainClass);
    }
    
    return args;
}

// CrashAnalyzer implementation
std::optional<CrashAnalyzer::CrashReport> CrashAnalyzer::parseCrashReport(const std::filesystem::path& crashLogPath) {
    if (!std::filesystem::exists(crashLogPath)) {
        return std::nullopt;
    }
    
    CrashReport report;
    std::ifstream file(crashLogPath);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    // Parse crash report
    std::regex titleRegex(R"(---- Minecraft Crash Report ----\n// (.+))");
    std::regex descRegex(R"(Description: (.+))");
    
    std::smatch match;
    if (std::regex_search(content, match, titleRegex)) {
        report.title = match[1];
    }
    if (std::regex_search(content, match, descRegex)) {
        report.description = match[1];
    }
    
    report.timestamp = std::chrono::system_clock::now();
    report.suggestions = analyzeCause(report);
    
    return report;
}

std::vector<std::string> CrashAnalyzer::analyzeCause(const CrashReport& report) {
    std::vector<std::string> suggestions;
    
    // Analyze common crash causes
    if (report.description.find("OutOfMemoryError") != std::string::npos) {
        suggestions.push_back("Increase allocated RAM in profile settings");
        suggestions.push_back("Close other applications to free memory");
        suggestions.push_back("Use 64-bit Java if not already");
    }
    
    if (report.description.find("NoSuchMethodError") != std::string::npos ||
        report.description.find("NoSuchFieldError") != std::string::npos) {
        suggestions.push_back("Mod version incompatibility detected");
        suggestions.push_back("Update all mods to compatible versions");
        suggestions.push_back("Check mod loader version compatibility");
    }
    
    if (report.description.find("MixinApply") != std::string::npos) {
        suggestions.push_back("Mixin conflict between mods");
        suggestions.push_back("Try disabling recently added mods");
    }
    
    return suggestions;
}

} // namespace konami::launcher
