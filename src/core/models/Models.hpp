// Konami Client - Data Models
// Core data structures used throughout the application

#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <optional>
#include <nlohmann/json.hpp>

namespace konami {

using json = nlohmann::json;

//=============================================================================
// Account Models
//=============================================================================

enum class AccountType {
    Microsoft,
    Offline
};

struct Account {
    std::string id;
    std::string username;
    std::string uuid;
    std::string avatarUrl;
    AccountType type;
    std::string accessToken;
    std::string refreshToken;
    std::chrono::system_clock::time_point expiresAt;
    bool isActive{false};
    
    bool isExpired() const {
        return std::chrono::system_clock::now() >= expiresAt;
    }
    
    // JSON serialization
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Account, id, username, uuid, avatarUrl)
};

struct XboxToken {
    std::string token;
    std::string userHash;
    std::chrono::system_clock::time_point expiresAt;
};

struct MinecraftToken {
    std::string accessToken;
    std::string tokenType;
    int expiresIn;
    std::string username;
    std::string uuid;
};

//=============================================================================
// Profile Models
//=============================================================================

enum class LoaderType {
    Vanilla,
    Fabric,
    Forge,
    Quilt,
    NeoForge
};

struct Profile {
    std::string id;
    std::string name;
    std::string gameVersion;
    std::string loader;
    std::string loaderVersion;
    std::string icon;
    std::string lastPlayed;
    std::string totalPlaytime;
    int modCount{0};
    bool isFavorite{false};
    std::string createdAt;
    
    // Java settings
    std::string javaPath;
    int minMemory{1024};
    int maxMemory{4096};
    std::string jvmArgs;
    
    // Game settings
    int windowWidth{854};
    int windowHeight{480};
    bool fullscreen{false};
    std::string gameDirectory;
    
    // Mod list
    std::vector<std::string> enabledMods;
    
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Profile, id, name, gameVersion, loader, 
                                    loaderVersion, icon, isFavorite, createdAt,
                                    javaPath, minMemory, maxMemory, jvmArgs,
                                    windowWidth, windowHeight, fullscreen,
                                    gameDirectory, enabledMods)
};

//=============================================================================
// Mod Models
//=============================================================================

enum class ModSource {
    Modrinth,
    CurseForge,
    Local
};

struct ModVersion {
    std::string id;
    std::string versionNumber;
    std::string gameVersions;
    std::string loaders;
    std::string downloadUrl;
    std::string filename;
    int fileSize;
    std::string sha512;
    std::string changelog;
    std::string releaseDate;
    bool featured{false};
};

struct Mod {
    std::string id;
    std::string slug;
    std::string name;
    std::string author;
    std::string description;
    std::string version;
    std::string gameVersion;
    int downloads{0};
    std::string iconUrl;
    bool isInstalled{false};
    bool isEnabled{true};
    bool hasUpdate{false};
    std::string source; // "modrinth", "curseforge", "local"
    std::string category;
    std::string license;
    std::string websiteUrl;
    std::string sourceUrl;
    std::vector<std::string> categories;
    std::vector<ModVersion> versions;
    std::vector<std::string> dependencies;
    
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Mod, id, slug, name, author, description,
                                    version, gameVersion, downloads, iconUrl,
                                    isInstalled, isEnabled, source, category)
};

struct ModDependency {
    std::string modId;
    std::string versionId;
    bool required{true};
};

//=============================================================================
// Skin Models
//=============================================================================

enum class SkinModel {
    Classic,  // Steve
    Slim      // Alex
};

struct Skin {
    std::string id;
    std::string name;
    std::string textureUrl;
    std::string textureHash;
    std::string modelType; // "classic" or "slim"
    bool isActive{false};
    bool isFavorite{false};
    std::string createdAt;
    std::vector<uint8_t> textureData;
    
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Skin, id, name, textureUrl, modelType,
                                    isActive, isFavorite, createdAt)
};

struct Cape {
    std::string id;
    std::string name;
    std::string textureUrl;
    bool isActive{false};
    std::string source; // "minecraft", "optifine", "custom"
    
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Cape, id, name, textureUrl, isActive, source)
};

//=============================================================================
// Download Models
//=============================================================================

enum class DownloadStatus {
    Pending,
    Downloading,
    Paused,
    Completed,
    Failed,
    Cancelled
};

struct DownloadTask {
    std::string id;
    std::string url;
    std::string destination;
    std::string filename;
    std::string sha256;
    std::string sha512;
    int64_t totalSize{0};
    int64_t downloadedSize{0};
    DownloadStatus status{DownloadStatus::Pending};
    int retryCount{0};
    int maxRetries{3};
    std::string errorMessage;
    
    float progress() const {
        if (totalSize == 0) return 0.0f;
        return static_cast<float>(downloadedSize) / static_cast<float>(totalSize);
    }
};

struct DownloadQueue {
    std::vector<DownloadTask> tasks;
    int completedCount{0};
    int failedCount{0};
    int64_t totalBytes{0};
    int64_t downloadedBytes{0};
    std::chrono::steady_clock::time_point startTime;
    
    float totalProgress() const {
        if (totalBytes == 0) return 0.0f;
        return static_cast<float>(downloadedBytes) / static_cast<float>(totalBytes);
    }
};

//=============================================================================
// Version Models
//=============================================================================

enum class VersionType {
    Release,
    Snapshot,
    OldBeta,
    OldAlpha
};

struct GameVersion {
    std::string id;
    VersionType type;
    std::string url;
    std::string time;
    std::string releaseTime;
    std::string sha1;
    int complianceLevel{0};
    
    bool isRelease() const { return type == VersionType::Release; }
    bool isSnapshot() const { return type == VersionType::Snapshot; }
};

struct VersionManifest {
    std::string latestRelease;
    std::string latestSnapshot;
    std::vector<GameVersion> versions;
};

struct Library {
    std::string name;
    std::string url;
    std::string path;
    std::string sha1;
    int64_t size;
    bool isNative{false};
    std::string nativeClassifier;
    std::vector<std::string> rules;
};

struct AssetIndex {
    std::string id;
    std::string sha1;
    int64_t size;
    int64_t totalSize;
    std::string url;
};

struct VersionDetails {
    std::string id;
    VersionType type;
    std::string mainClass;
    std::string minecraftArguments;
    std::string inheritsFrom;
    AssetIndex assetIndex;
    std::vector<Library> libraries;
    std::string clientUrl;
    std::string clientSha1;
    int64_t clientSize;
    std::string javaVersion;
    int javaVersionMajor{8};
};

//=============================================================================
// News Models
//=============================================================================

struct NewsEntry {
    std::string id;
    std::string title;
    std::string summary;
    std::string content;
    std::string imageUrl;
    std::string date;
    std::string url;
    std::string category;
    bool isPinned{false};
};

//=============================================================================
// Settings Models
//=============================================================================

struct LauncherSettings {
    // General
    std::string language{"English"};
    bool autoUpdate{true};
    bool minimizeOnLaunch{true};
    bool closeOnLaunch{false};
    bool showNews{true};
    
    // Theme
    std::string themeName{"Konami Dark"};
    uint32_t accentColor{0x00d9ff};
    bool useBlurEffects{true};
    float animationSpeed{1.0f};
    
    // Java
    std::string javaPath{"auto"};
    int minMemory{1024};
    int maxMemory{4096};
    std::string jvmArgs;
    
    // Performance
    int concurrentDownloads{4};
    bool useCache{true};
    bool enableHardwareAcceleration{true};
    
    // Advanced
    std::string gameDirectory;
    bool keepLauncherOpen{true};
    bool showConsole{false};
    bool developerMode{false};
    
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(LauncherSettings, language, autoUpdate,
                                    minimizeOnLaunch, closeOnLaunch, showNews,
                                    themeName, accentColor, useBlurEffects,
                                    animationSpeed, javaPath, minMemory, maxMemory,
                                    jvmArgs, concurrentDownloads, useCache,
                                    gameDirectory, keepLauncherOpen, showConsole)
};

//=============================================================================
// Launch Models
//=============================================================================

struct LaunchOptions {
    std::string profileId;
    std::string gameVersion;
    std::string javaPath;
    int minMemory;
    int maxMemory;
    std::string jvmArgs;
    int windowWidth;
    int windowHeight;
    bool fullscreen;
    std::string gameDirectory;
    std::string accessToken;
    std::string uuid;
    std::string username;
    std::string userType;
    bool demoMode{false};
    std::vector<std::string> additionalMods;
};

struct LaunchResult {
    bool success{false};
    int exitCode{0};
    std::string errorMessage;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    std::string logFile;
};

//=============================================================================
// API Models
//=============================================================================

struct ModrinthProject {
    std::string id;
    std::string slug;
    std::string projectType;
    std::string team;
    std::string title;
    std::string description;
    std::string body;
    std::string iconUrl;
    std::string status;
    int downloads;
    int followers;
    std::vector<std::string> categories;
    std::vector<std::string> gameVersions;
    std::vector<std::string> loaders;
    std::string license;
    std::string published;
    std::string updated;
};

struct CurseForgeProject {
    int id;
    std::string name;
    std::string slug;
    std::string summary;
    int downloadCount;
    std::string dateCreated;
    std::string dateModified;
    std::string dateReleased;
    std::string logoUrl;
    int gameId;
    int categoryId;
};

} // namespace konami
