#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <filesystem>
#include <chrono>
#include <future>
#include "../profile/ProfileManager.hpp"

namespace konami::launcher {

// Launch state
enum class LaunchState {
    Idle,
    Preparing,
    DownloadingAssets,
    DownloadingLibraries,
    DownloadingClient,
    InstallingLoader,
    Building,
    Launching,
    Running,
    Crashed,
    Finished
};

// Process info
struct ProcessInfo {
    int pid = 0;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    int exitCode = 0;
    bool crashed = false;
    std::string crashReason;
};

// Launch options
struct LaunchOptions {
    std::string profileId;
    bool quickPlay = false;
    std::string quickPlayServer;    // server:port for direct connect
    std::string quickPlayWorld;     // world name for singleplayer
    bool offlineMode = false;
    std::string offlineUsername;
    bool demoMode = false;
    bool debug = false;
    std::vector<std::string> extraJvmArgs;
    std::vector<std::string> extraGameArgs;
};

// Progress info
struct LaunchProgress {
    LaunchState state = LaunchState::Idle;
    std::string message;
    double progress = 0.0;      // 0.0 - 1.0
    int downloadedFiles = 0;
    int totalFiles = 0;
    int64_t downloadedBytes = 0;
    int64_t totalBytes = 0;
};

// Version info from Mojang
struct VersionInfo {
    std::string id;
    std::string type;           // release, snapshot, old_beta, old_alpha
    std::string url;
    std::string time;
    std::string releaseTime;
    std::string sha1;
    bool complianceLevel = true;
};

// Version manifest
struct VersionManifest {
    struct Latest {
        std::string release;
        std::string snapshot;
    } latest;
    
    std::vector<VersionInfo> versions;
};

// Library info
struct LibraryInfo {
    std::string name;           // group:artifact:version
    std::string path;
    std::string url;
    std::string sha1;
    int64_t size = 0;
    bool native = false;
    std::string nativeClassifier;
    
    struct Rule {
        std::string action;     // allow/disallow
        struct OS {
            std::string name;
            std::string version;
            std::string arch;
        } os;
    };
    std::vector<Rule> rules;
};

// Asset info
struct AssetInfo {
    std::string hash;
    int64_t size = 0;
};

// Game output handler
using OutputCallback = std::function<void(const std::string& line, bool isError)>;
using ProgressCallback = std::function<void(const LaunchProgress& progress)>;
using StateCallback = std::function<void(LaunchState state)>;

// Main Game Launcher class
class GameLauncher {
public:
    GameLauncher();
    ~GameLauncher();
    
    // Initialization
    bool initialize(const std::filesystem::path& gameDirectory);
    void shutdown();
    
    // Version management
    std::future<VersionManifest> fetchVersionManifest();
    std::vector<VersionInfo> getAvailableVersions() const;
    std::vector<VersionInfo> getInstalledVersions() const;
    bool isVersionInstalled(const std::string& version) const;
    
    // Version installation
    std::future<bool> installVersion(const std::string& version, ProgressCallback progressCallback = nullptr);
    std::future<bool> installForge(const std::string& mcVersion, const std::string& forgeVersion, ProgressCallback progressCallback = nullptr);
    std::future<bool> installFabric(const std::string& mcVersion, const std::string& loaderVersion, ProgressCallback progressCallback = nullptr);
    std::future<bool> installQuilt(const std::string& mcVersion, const std::string& loaderVersion, ProgressCallback progressCallback = nullptr);
    std::future<bool> installNeoForge(const std::string& mcVersion, const std::string& neoforgeVersion, ProgressCallback progressCallback = nullptr);
    
    // Launching
    std::future<bool> launch(const LaunchOptions& options, ProgressCallback progressCallback = nullptr);
    std::future<bool> launchProfile(const std::string& profileId, ProgressCallback progressCallback = nullptr);
    
    // Process management
    bool isRunning() const;
    ProcessInfo getProcessInfo() const;
    void kill();
    void forceKill();
    
    // Output handling
    void setOutputCallback(OutputCallback callback);
    std::vector<std::string> getGameLog() const;
    void clearGameLog();
    
    // State management
    LaunchState getState() const;
    void setStateCallback(StateCallback callback);
    
    // Asset verification
    std::future<bool> verifyAssets(const std::string& version, ProgressCallback progressCallback = nullptr);
    std::future<bool> repairAssets(const std::string& version, ProgressCallback progressCallback = nullptr);
    
    // Configuration
    void setGameDirectory(const std::filesystem::path& path);
    std::filesystem::path getGameDirectory() const;
    void setAssetsDirectory(const std::filesystem::path& path);
    void setLibrariesDirectory(const std::filesystem::path& path);
    
    // Events
    void setOnGameStarted(std::function<void()> callback);
    void setOnGameExited(std::function<void(int exitCode)> callback);
    void setOnGameCrashed(std::function<void(const std::string& reason)> callback);
    
private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    
    // Internal methods
    std::vector<std::string> buildJvmArguments(const profile::Profile& profile, const LaunchOptions& options);
    std::vector<std::string> buildGameArguments(const profile::Profile& profile, const LaunchOptions& options);
    std::string buildClasspath(const std::string& version);
    bool extractNatives(const std::string& version);
    bool downloadLibraries(const std::string& version, ProgressCallback progressCallback);
    bool downloadAssets(const std::string& version, ProgressCallback progressCallback);
};

// Crash analyzer
class CrashAnalyzer {
public:
    struct CrashReport {
        std::string title;
        std::string description;
        std::string stackTrace;
        std::vector<std::string> modList;
        std::string javaVersion;
        std::string mcVersion;
        std::string loaderVersion;
        std::chrono::system_clock::time_point timestamp;
        
        std::string possibleCause;
        std::vector<std::string> suggestions;
    };
    
    static std::optional<CrashReport> parseCrashReport(const std::filesystem::path& crashLogPath);
    static std::optional<CrashReport> analyzeLatestCrash(const std::filesystem::path& crashReportsDir);
    static std::vector<std::string> analyzeCause(const CrashReport& report);
};

// JVM argument builder
class JvmArgumentBuilder {
public:
    JvmArgumentBuilder& withMemory(int minMB, int maxMB);
    JvmArgumentBuilder& withGC(const std::string& gcType);
    JvmArgumentBuilder& withNatives(const std::filesystem::path& nativesPath);
    JvmArgumentBuilder& withClasspath(const std::string& classpath);
    JvmArgumentBuilder& withMainClass(const std::string& mainClass);
    JvmArgumentBuilder& withProperty(const std::string& key, const std::string& value);
    JvmArgumentBuilder& withCustomArg(const std::string& arg);
    JvmArgumentBuilder& withGCLogging(bool enabled);
    JvmArgumentBuilder& withLargePages(bool enabled);
    
    std::vector<std::string> build() const;
    
private:
    int m_minMemory = 1024;
    int m_maxMemory = 4096;
    std::string m_gcType = "G1GC";
    std::filesystem::path m_nativesPath;
    std::string m_classpath;
    std::string m_mainClass;
    std::vector<std::pair<std::string, std::string>> m_properties;
    std::vector<std::string> m_customArgs;
    bool m_gcLogging = false;
    bool m_largePages = false;
};

} // namespace konami::launcher
