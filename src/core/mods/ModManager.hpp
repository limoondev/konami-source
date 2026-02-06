#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <filesystem>
#include <unordered_map>
#include <optional>
#include <future>
#include <nlohmann/json.hpp>

namespace konami::mods {

// Mod loader types
enum class ModLoader {
    None,
    Forge,
    Fabric,
    Quilt,
    LiteLoader,
    NeoForge
};

// Mod source platforms
enum class ModSource {
    Local,
    CurseForge,
    Modrinth,
    GitHub,
    Custom
};

// Mod dependency type
enum class DependencyType {
    Required,
    Optional,
    Incompatible,
    Embedded
};

// Mod information structure
struct ModInfo {
    std::string id;
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    std::string website;
    std::string iconPath;
    std::string filePath;
    std::string sha256Hash;
    
    ModLoader loader = ModLoader::None;
    ModSource source = ModSource::Local;
    
    std::vector<std::string> gameVersions;
    std::vector<std::string> categories;
    
    int64_t fileSize = 0;
    int64_t downloads = 0;
    
    bool enabled = true;
    bool hasUpdate = false;
    std::string latestVersion;
    
    nlohmann::json toJson() const;
    static ModInfo fromJson(const nlohmann::json& j);
};

// Mod dependency
struct ModDependency {
    std::string modId;
    std::string versionRange;
    DependencyType type = DependencyType::Required;
    bool resolved = false;
};

// Conflict information
struct ModConflict {
    std::string modId1;
    std::string modId2;
    std::string reason;
    enum class Severity { Warning, Error, Critical } severity;
};

// Search filter for mod APIs
struct ModSearchFilter {
    std::string query;
    std::string gameVersion;
    ModLoader loader = ModLoader::None;
    std::vector<std::string> categories;
    int page = 0;
    int pageSize = 20;
    
    enum class SortBy { Relevance, Downloads, Updated, Name } sortBy = SortBy::Relevance;
};

// Search result
struct ModSearchResult {
    std::vector<ModInfo> mods;
    int totalCount = 0;
    int currentPage = 0;
    bool hasMore = false;
};

// Download progress callback
using DownloadProgressCallback = std::function<void(const std::string& modId, double progress, int64_t downloaded, int64_t total)>;

// Main Mod Manager class
class ModManager {
public:
    ModManager();
    ~ModManager();
    
    // Initialization
    bool initialize(const std::filesystem::path& modsDirectory);
    void shutdown();
    
    // Mod discovery and loading
    std::vector<ModInfo> scanInstalledMods();
    std::optional<ModInfo> parseModFile(const std::filesystem::path& modPath);
    bool refreshModList();
    
    // Mod management
    bool enableMod(const std::string& modId);
    bool disableMod(const std::string& modId);
    bool deleteMod(const std::string& modId);
    bool moveMod(const std::string& modId, const std::filesystem::path& newPath);
    
    // Mod installation
    std::future<bool> installMod(const ModInfo& mod, DownloadProgressCallback progressCallback = nullptr);
    std::future<bool> installModFromUrl(const std::string& url, DownloadProgressCallback progressCallback = nullptr);
    std::future<bool> installModFromFile(const std::filesystem::path& filePath);
    bool uninstallMod(const std::string& modId);
    
    // Mod updates
    std::future<std::vector<ModInfo>> checkForUpdates();
    std::future<bool> updateMod(const std::string& modId, DownloadProgressCallback progressCallback = nullptr);
    std::future<bool> updateAllMods(DownloadProgressCallback progressCallback = nullptr);
    
    // Dependency management
    std::vector<ModDependency> getDependencies(const std::string& modId);
    std::vector<ModDependency> getUnresolvedDependencies();
    std::future<bool> resolveDependencies(const std::string& modId);
    std::future<bool> resolveAllDependencies();
    
    // Conflict detection
    std::vector<ModConflict> detectConflicts();
    bool hasConflicts() const;
    std::vector<ModConflict> getConflictsForMod(const std::string& modId);
    
    // Search APIs
    std::future<ModSearchResult> searchCurseForge(const ModSearchFilter& filter);
    std::future<ModSearchResult> searchModrinth(const ModSearchFilter& filter);
    std::future<ModSearchResult> searchAll(const ModSearchFilter& filter);
    
    // Mod information
    std::vector<ModInfo> getInstalledMods() const;
    std::optional<ModInfo> getModInfo(const std::string& modId) const;
    bool isModInstalled(const std::string& modId) const;
    bool isModEnabled(const std::string& modId) const;
    
    // Loader management
    std::vector<ModLoader> getInstalledLoaders() const;
    bool isLoaderInstalled(ModLoader loader) const;
    std::future<bool> installLoader(ModLoader loader, const std::string& gameVersion);
    
    // Configuration
    void setModsDirectory(const std::filesystem::path& path);
    std::filesystem::path getModsDirectory() const;
    void setCacheDirectory(const std::filesystem::path& path);
    
    // Export/Import
    bool exportModList(const std::filesystem::path& outputPath);
    std::future<bool> importModList(const std::filesystem::path& inputPath);
    
    // Events
    void setOnModInstalled(std::function<void(const ModInfo&)> callback);
    void setOnModRemoved(std::function<void(const std::string&)> callback);
    void setOnModUpdated(std::function<void(const ModInfo&)> callback);
    void setOnConflictDetected(std::function<void(const ModConflict&)> callback);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

// CurseForge API client
class CurseForgeClient {
public:
    CurseForgeClient(const std::string& apiKey);
    ~CurseForgeClient();
    
    std::future<ModSearchResult> search(const ModSearchFilter& filter);
    std::future<std::optional<ModInfo>> getModInfo(int projectId);
    std::future<std::vector<ModInfo>> getModFiles(int projectId, const std::string& gameVersion = "");
    std::future<std::string> getDownloadUrl(int fileId);
    
    void setApiKey(const std::string& key);
    bool isApiKeyValid() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

// Modrinth API client
class ModrinthClient {
public:
    ModrinthClient();
    ~ModrinthClient();
    
    std::future<ModSearchResult> search(const ModSearchFilter& filter);
    std::future<std::optional<ModInfo>> getProject(const std::string& projectId);
    std::future<std::vector<ModInfo>> getProjectVersions(const std::string& projectId, const std::string& gameVersion = "");
    std::future<std::string> getDownloadUrl(const std::string& versionId);
    
    void setUserAgent(const std::string& userAgent);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

// Utility functions
std::string modLoaderToString(ModLoader loader);
ModLoader stringToModLoader(const std::string& str);
std::string modSourceToString(ModSource source);
ModSource stringToModSource(const std::string& str);

} // namespace konami::mods
