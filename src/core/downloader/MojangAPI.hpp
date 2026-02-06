#pragma once

/**
 * MojangAPI.hpp
 * 
 * Interface to Mojang's official Minecraft APIs.
 * Handles version manifests, game downloads, and assets.
 */

#include <string>
#include <vector>
#include <optional>
#include <future>

#include <nlohmann/json.hpp>

namespace nexus::core::downloader {

/**
 * Version type enum
 */
enum class VersionType {
    Release,
    Snapshot,
    OldBeta,
    OldAlpha
};

/**
 * Version info from manifest
 */
struct VersionInfo {
    std::string id;
    VersionType type;
    std::string url;
    std::string time;
    std::string releaseTime;
    std::string sha1;
    int complianceLevel{0};
};

/**
 * Download info from version JSON
 */
struct DownloadInfo {
    std::string url;
    std::string sha1;
    size_t size{0};
};

/**
 * Library info
 */
struct LibraryInfo {
    std::string name;
    DownloadInfo download;
    std::vector<std::string> rules;
    std::string nativesClassifier;
    bool isNative{false};
};

/**
 * Asset index info
 */
struct AssetIndex {
    std::string id;
    std::string url;
    std::string sha1;
    size_t size{0};
    size_t totalSize{0};
};

/**
 * Full version data
 */
struct VersionData {
    std::string id;
    VersionType type;
    std::string mainClass;
    std::string minecraftArguments;
    std::string javaVersion;
    int javaVersionMajor{8};
    
    AssetIndex assetIndex;
    DownloadInfo clientDownload;
    DownloadInfo serverDownload;
    std::vector<LibraryInfo> libraries;
    
    nlohmann::json rawJson;
};

/**
 * Asset object
 */
struct AssetObject {
    std::string name;
    std::string hash;
    size_t size{0};
};

/**
 * MojangAPI - Mojang API client
 */
class MojangAPI {
public:
    // API endpoints
    static constexpr const char* VERSION_MANIFEST_URL = 
        "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json";
    static constexpr const char* RESOURCES_URL = 
        "https://resources.download.minecraft.net";
    static constexpr const char* LIBRARIES_URL = 
        "https://libraries.minecraft.net";
    
    /**
     * Constructor
     */
    MojangAPI() = default;
    
    /**
     * Get version manifest
     * @return Future with version list
     */
    std::future<std::vector<VersionInfo>> getVersionManifest();
    
    /**
     * Get latest release version
     * @return Future with version info
     */
    std::future<std::optional<VersionInfo>> getLatestRelease();
    
    /**
     * Get latest snapshot version
     * @return Future with version info
     */
    std::future<std::optional<VersionInfo>> getLatestSnapshot();
    
    /**
     * Get version data
     * @param versionInfo Version info from manifest
     * @return Future with full version data
     */
    std::future<std::optional<VersionData>> getVersionData(const VersionInfo& versionInfo);
    
    /**
     * Get version data by ID
     * @param versionId Version ID
     * @return Future with full version data
     */
    std::future<std::optional<VersionData>> getVersionDataById(const std::string& versionId);
    
    /**
     * Get asset index
     * @param assetIndex Asset index info
     * @return Future with asset objects
     */
    std::future<std::vector<AssetObject>> getAssetIndex(const AssetIndex& assetIndex);
    
    /**
     * Build asset download URL
     * @param asset Asset object
     * @return Download URL
     */
    static std::string getAssetUrl(const AssetObject& asset);
    
    /**
     * Build library download URL
     * @param library Library info
     * @return Download URL
     */
    static std::string getLibraryUrl(const LibraryInfo& library);
    
    /**
     * Get library path (Maven-style)
     * @param name Library name (e.g., "com.google.guava:guava:21.0")
     * @return Library path
     */
    static std::string getLibraryPath(const std::string& name);
    
    /**
     * Check if library applies to current platform
     * @param library Library info
     * @return true if library should be used
     */
    static bool libraryAppliesToPlatform(const LibraryInfo& library);

private:
    /**
     * Parse version type from string
     * @param type Type string
     * @return VersionType enum
     */
    static VersionType parseVersionType(const std::string& type);
    
    /**
     * Parse version info from JSON
     * @param json JSON object
     * @return VersionInfo
     */
    static VersionInfo parseVersionInfo(const nlohmann::json& json);
    
    /**
     * Parse version data from JSON
     * @param json JSON object
     * @return VersionData
     */
    static VersionData parseVersionData(const nlohmann::json& json);
    
    /**
     * Parse library from JSON
     * @param json JSON object
     * @return LibraryInfo
     */
    static LibraryInfo parseLibrary(const nlohmann::json& json);
    
    /**
     * Parse asset object from JSON
     * @param name Asset name
     * @param json JSON object
     * @return AssetObject
     */
    static AssetObject parseAssetObject(const std::string& name, const nlohmann::json& json);

private:
    std::vector<VersionInfo> m_cachedManifest;
    std::string m_latestRelease;
    std::string m_latestSnapshot;
};

} // namespace nexus::core::downloader
