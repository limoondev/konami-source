#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <filesystem>
#include <functional>
#include <chrono>
#include <nlohmann/json.hpp>

namespace konami::profile {

// Java runtime configuration
struct JavaConfig {
    std::string path;
    std::string version;
    int minMemoryMB = 1024;
    int maxMemoryMB = 4096;
    std::vector<std::string> jvmArgs;
    bool autoDetect = true;
    
    nlohmann::json toJson() const;
    static JavaConfig fromJson(const nlohmann::json& j);
};

// Resolution settings
struct ResolutionConfig {
    int width = 1280;
    int height = 720;
    bool fullscreen = false;
    bool maximized = false;
    
    nlohmann::json toJson() const;
    static ResolutionConfig fromJson(const nlohmann::json& j);
};

// Mod loader configuration
struct LoaderConfig {
    std::string type; // forge, fabric, quilt, vanilla
    std::string version;
    bool installed = false;
    
    nlohmann::json toJson() const;
    static LoaderConfig fromJson(const nlohmann::json& j);
};

// Profile snapshot for rollback
struct ProfileSnapshot {
    std::string id;
    std::string name;
    std::string description;
    std::chrono::system_clock::time_point createdAt;
    std::filesystem::path dataPath;
    
    nlohmann::json toJson() const;
    static ProfileSnapshot fromJson(const nlohmann::json& j);
};

// Main profile structure
struct Profile {
    std::string id;
    std::string name;
    std::string iconPath;
    std::string gameVersion;
    std::string gameDirectory;
    
    JavaConfig javaConfig;
    ResolutionConfig resolution;
    LoaderConfig loader;
    
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point lastPlayed;
    int64_t totalPlayTime = 0; // in seconds
    
    bool quickLaunch = false;
    bool showInMenu = true;
    int sortOrder = 0;
    
    std::vector<std::string> enabledMods;
    std::vector<ProfileSnapshot> snapshots;
    
    nlohmann::json toJson() const;
    static Profile fromJson(const nlohmann::json& j);
};

// Profile import/export format
enum class ProfileFormat {
    KonamiProfile,  // .kprofile
    MultiMC,        // instance.cfg
    CurseForge,     // manifest.json
    Modrinth,       // .mrpack
    ATLauncher,     // instance.json
    Prism           // mmc-pack.json
};

// Profile Manager class
class ProfileManager {
public:
    ProfileManager();
    ~ProfileManager();
    
    // Initialization
    bool initialize(const std::filesystem::path& profilesDirectory);
    void shutdown();
    
    // Profile CRUD
    Profile createProfile(const std::string& name, const std::string& gameVersion);
    bool updateProfile(const Profile& profile);
    bool deleteProfile(const std::string& profileId);
    std::optional<Profile> duplicateProfile(const std::string& profileId, const std::string& newName);
    
    // Profile retrieval
    std::vector<Profile> getAllProfiles() const;
    std::optional<Profile> getProfile(const std::string& profileId) const;
    std::optional<Profile> getProfileByName(const std::string& name) const;
    std::optional<Profile> getActiveProfile() const;
    
    // Active profile management
    bool setActiveProfile(const std::string& profileId);
    std::string getActiveProfileId() const;
    
    // Profile validation
    bool validateProfile(const std::string& profileId) const;
    std::vector<std::string> getProfileIssues(const std::string& profileId) const;
    
    // Game directory management
    std::filesystem::path getProfileDirectory(const std::string& profileId) const;
    std::filesystem::path getModsDirectory(const std::string& profileId) const;
    std::filesystem::path getSavesDirectory(const std::string& profileId) const;
    std::filesystem::path getResourcePacksDirectory(const std::string& profileId) const;
    std::filesystem::path getShaderPacksDirectory(const std::string& profileId) const;
    
    // Snapshots and rollback
    ProfileSnapshot createSnapshot(const std::string& profileId, const std::string& name, const std::string& description = "");
    bool restoreSnapshot(const std::string& profileId, const std::string& snapshotId);
    bool deleteSnapshot(const std::string& profileId, const std::string& snapshotId);
    std::vector<ProfileSnapshot> getSnapshots(const std::string& profileId) const;
    
    // Import/Export
    std::optional<Profile> importProfile(const std::filesystem::path& path, ProfileFormat format = ProfileFormat::KonamiProfile);
    bool exportProfile(const std::string& profileId, const std::filesystem::path& outputPath, ProfileFormat format = ProfileFormat::KonamiProfile);
    bool importModpack(const std::filesystem::path& mrpackPath);
    bool exportModpack(const std::string& profileId, const std::filesystem::path& outputPath);
    
    // Java detection
    std::vector<JavaConfig> detectInstalledJava();
    bool downloadJava(const std::string& version, std::function<void(double)> progressCallback = nullptr);
    
    // Statistics
    void updatePlayTime(const std::string& profileId, int64_t sessionSeconds);
    int64_t getTotalPlayTime(const std::string& profileId) const;
    std::chrono::system_clock::time_point getLastPlayed(const std::string& profileId) const;
    
    // Events
    void setOnProfileCreated(std::function<void(const Profile&)> callback);
    void setOnProfileUpdated(std::function<void(const Profile&)> callback);
    void setOnProfileDeleted(std::function<void(const std::string&)> callback);
    void setOnActiveProfileChanged(std::function<void(const std::string&)> callback);
    
    // Configuration
    void setProfilesDirectory(const std::filesystem::path& path);
    std::filesystem::path getProfilesDirectory() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    
    void saveProfiles();
    void loadProfiles();
    std::string generateProfileId();
};

// Profile builder for fluent API
class ProfileBuilder {
public:
    ProfileBuilder(const std::string& name, const std::string& gameVersion);
    
    ProfileBuilder& withIcon(const std::string& iconPath);
    ProfileBuilder& withJava(const JavaConfig& config);
    ProfileBuilder& withResolution(int width, int height, bool fullscreen = false);
    ProfileBuilder& withLoader(const std::string& type, const std::string& version);
    ProfileBuilder& withMods(const std::vector<std::string>& mods);
    ProfileBuilder& withGameDirectory(const std::string& path);
    ProfileBuilder& asQuickLaunch(bool enabled = true);
    
    Profile build();

private:
    Profile m_profile;
};

} // namespace konami::profile
