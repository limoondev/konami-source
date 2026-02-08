#include "ProfileManager.hpp"
#include "../Logger.hpp"
#include <fstream>
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace konami::profile {

// JSON serialization implementations
nlohmann::json JavaConfig::toJson() const {
    return {
        {"path", path},
        {"version", version},
        {"minMemoryMB", minMemoryMB},
        {"maxMemoryMB", maxMemoryMB},
        {"jvmArgs", jvmArgs},
        {"autoDetect", autoDetect}
    };
}

JavaConfig JavaConfig::fromJson(const nlohmann::json& j) {
    JavaConfig config;
    config.path = j.value("path", "");
    config.version = j.value("version", "");
    config.minMemoryMB = j.value("minMemoryMB", 1024);
    config.maxMemoryMB = j.value("maxMemoryMB", 4096);
    config.jvmArgs = j.value("jvmArgs", std::vector<std::string>{});
    config.autoDetect = j.value("autoDetect", true);
    return config;
}

nlohmann::json ResolutionConfig::toJson() const {
    return {
        {"width", width},
        {"height", height},
        {"fullscreen", fullscreen},
        {"maximized", maximized}
    };
}

ResolutionConfig ResolutionConfig::fromJson(const nlohmann::json& j) {
    ResolutionConfig config;
    config.width = j.value("width", 1280);
    config.height = j.value("height", 720);
    config.fullscreen = j.value("fullscreen", false);
    config.maximized = j.value("maximized", false);
    return config;
}

nlohmann::json LoaderConfig::toJson() const {
    return {
        {"type", type},
        {"version", version},
        {"installed", installed}
    };
}

LoaderConfig LoaderConfig::fromJson(const nlohmann::json& j) {
    LoaderConfig config;
    config.type = j.value("type", "vanilla");
    config.version = j.value("version", "");
    config.installed = j.value("installed", false);
    return config;
}

nlohmann::json ProfileSnapshot::toJson() const {
    return {
        {"id", id},
        {"name", name},
        {"description", description},
        {"createdAt", std::chrono::duration_cast<std::chrono::seconds>(
            createdAt.time_since_epoch()).count()},
        {"dataPath", dataPath.string()}
    };
}

ProfileSnapshot ProfileSnapshot::fromJson(const nlohmann::json& j) {
    ProfileSnapshot snapshot;
    snapshot.id = j.value("id", "");
    snapshot.name = j.value("name", "");
    snapshot.description = j.value("description", "");
    snapshot.createdAt = std::chrono::system_clock::time_point(
        std::chrono::seconds(j.value("createdAt", 0LL)));
    snapshot.dataPath = j.value("dataPath", "");
    return snapshot;
}

nlohmann::json Profile::toJson() const {
    nlohmann::json j = {
        {"id", id},
        {"name", name},
        {"iconPath", iconPath},
        {"gameVersion", gameVersion},
        {"gameDirectory", gameDirectory},
        {"javaConfig", javaConfig.toJson()},
        {"resolution", resolution.toJson()},
        {"loader", loader.toJson()},
        {"createdAt", std::chrono::duration_cast<std::chrono::seconds>(
            createdAt.time_since_epoch()).count()},
        {"lastPlayed", std::chrono::duration_cast<std::chrono::seconds>(
            lastPlayed.time_since_epoch()).count()},
        {"totalPlayTime", totalPlayTime},
        {"quickLaunch", quickLaunch},
        {"showInMenu", showInMenu},
        {"sortOrder", sortOrder},
        {"enabledMods", enabledMods}
    };
    
    nlohmann::json snapshotsJson = nlohmann::json::array();
    for (const auto& snapshot : snapshots) {
        snapshotsJson.push_back(snapshot.toJson());
    }
    j["snapshots"] = snapshotsJson;
    
    return j;
}

Profile Profile::fromJson(const nlohmann::json& j) {
    Profile profile;
    profile.id = j.value("id", "");
    profile.name = j.value("name", "");
    profile.iconPath = j.value("iconPath", "");
    profile.gameVersion = j.value("gameVersion", "");
    profile.gameDirectory = j.value("gameDirectory", "");
    
    if (j.contains("javaConfig")) {
        profile.javaConfig = JavaConfig::fromJson(j["javaConfig"]);
    }
    if (j.contains("resolution")) {
        profile.resolution = ResolutionConfig::fromJson(j["resolution"]);
    }
    if (j.contains("loader")) {
        profile.loader = LoaderConfig::fromJson(j["loader"]);
    }
    
    profile.createdAt = std::chrono::system_clock::time_point(
        std::chrono::seconds(j.value("createdAt", 0LL)));
    profile.lastPlayed = std::chrono::system_clock::time_point(
        std::chrono::seconds(j.value("lastPlayed", 0LL)));
    profile.totalPlayTime = j.value("totalPlayTime", 0LL);
    profile.quickLaunch = j.value("quickLaunch", false);
    profile.showInMenu = j.value("showInMenu", true);
    profile.sortOrder = j.value("sortOrder", 0);
    profile.enabledMods = j.value("enabledMods", std::vector<std::string>{});
    
    if (j.contains("snapshots") && j["snapshots"].is_array()) {
        for (const auto& s : j["snapshots"]) {
            profile.snapshots.push_back(ProfileSnapshot::fromJson(s));
        }
    }
    
    return profile;
}

// ProfileManager Implementation
class ProfileManager::Impl {
public:
    std::filesystem::path profilesDirectory;
    std::vector<Profile> profiles;
    std::string activeProfileId;
    
    std::function<void(const Profile&)> onProfileCreated;
    std::function<void(const Profile&)> onProfileUpdated;
    std::function<void(const std::string&)> onProfileDeleted;
    std::function<void(const std::string&)> onActiveProfileChanged;
    
    mutable std::mutex profilesMutex;
};

ProfileManager::ProfileManager() : m_impl(std::make_unique<Impl>()) {}

ProfileManager::~ProfileManager() {
    shutdown();
}

bool ProfileManager::initialize(const std::filesystem::path& profilesDirectory) {
    m_impl->profilesDirectory = profilesDirectory;
    
    if (!std::filesystem::exists(profilesDirectory)) {
        std::filesystem::create_directories(profilesDirectory);
    }
    
    loadProfiles();
    
    core::Logger::instance().info( "Initialized with {} profiles", m_impl->profiles.size());
    return true;
}

void ProfileManager::shutdown() {
    saveProfiles();
}

Profile ProfileManager::createProfile(const std::string& name, const std::string& gameVersion) {
    std::lock_guard<std::mutex> lock(m_impl->profilesMutex);
    
    Profile profile;
    profile.id = generateProfileId();
    profile.name = name;
    profile.gameVersion = gameVersion;
    profile.createdAt = std::chrono::system_clock::now();
    profile.gameDirectory = (m_impl->profilesDirectory / profile.id).string();
    profile.loader.type = "vanilla";
    
    // Create profile directory
    std::filesystem::create_directories(profile.gameDirectory);
    std::filesystem::create_directories(std::filesystem::path(profile.gameDirectory) / "mods");
    std::filesystem::create_directories(std::filesystem::path(profile.gameDirectory) / "saves");
    std::filesystem::create_directories(std::filesystem::path(profile.gameDirectory) / "resourcepacks");
    std::filesystem::create_directories(std::filesystem::path(profile.gameDirectory) / "shaderpacks");
    
    m_impl->profiles.push_back(profile);
    saveProfiles();
    
    if (m_impl->onProfileCreated) {
        m_impl->onProfileCreated(profile);
    }
    
    core::Logger::instance().info( "Created profile: {} ({})", name, profile.id);
    return profile;
}

bool ProfileManager::updateProfile(const Profile& profile) {
    std::lock_guard<std::mutex> lock(m_impl->profilesMutex);
    
    auto it = std::find_if(m_impl->profiles.begin(), m_impl->profiles.end(),
        [&profile](const Profile& p) { return p.id == profile.id; });
    
    if (it != m_impl->profiles.end()) {
        *it = profile;
        saveProfiles();
        
        if (m_impl->onProfileUpdated) {
            m_impl->onProfileUpdated(profile);
        }
        
        return true;
    }
    
    return false;
}

bool ProfileManager::deleteProfile(const std::string& profileId) {
    std::lock_guard<std::mutex> lock(m_impl->profilesMutex);
    
    auto it = std::find_if(m_impl->profiles.begin(), m_impl->profiles.end(),
        [&profileId](const Profile& p) { return p.id == profileId; });
    
    if (it != m_impl->profiles.end()) {
        // Remove profile directory
        std::error_code ec;
        std::filesystem::remove_all(it->gameDirectory, ec);
        
        m_impl->profiles.erase(it);
        
        if (m_impl->activeProfileId == profileId) {
            m_impl->activeProfileId = m_impl->profiles.empty() ? "" : m_impl->profiles[0].id;
        }
        
        saveProfiles();
        
        if (m_impl->onProfileDeleted) {
            m_impl->onProfileDeleted(profileId);
        }
        
        core::Logger::instance().info( "Deleted profile: {}", profileId);
        return true;
    }
    
    return false;
}

std::optional<Profile> ProfileManager::duplicateProfile(const std::string& profileId, const std::string& newName) {
    auto original = getProfile(profileId);
    if (!original) return std::nullopt;
    
    Profile newProfile = *original;
    newProfile.id = generateProfileId();
    newProfile.name = newName;
    newProfile.createdAt = std::chrono::system_clock::now();
    newProfile.lastPlayed = std::chrono::system_clock::time_point{};
    newProfile.totalPlayTime = 0;
    newProfile.snapshots.clear();
    newProfile.gameDirectory = (m_impl->profilesDirectory / newProfile.id).string();
    
    // Copy directory contents
    std::error_code ec;
    std::filesystem::copy(original->gameDirectory, newProfile.gameDirectory, 
        std::filesystem::copy_options::recursive, ec);
    
    {
        std::lock_guard<std::mutex> lock(m_impl->profilesMutex);
        m_impl->profiles.push_back(newProfile);
        saveProfiles();
    }
    
    if (m_impl->onProfileCreated) {
        m_impl->onProfileCreated(newProfile);
    }
    
    return newProfile;
}

std::vector<Profile> ProfileManager::getAllProfiles() const {
    std::lock_guard<std::mutex> lock(m_impl->profilesMutex);
    return m_impl->profiles;
}

std::optional<Profile> ProfileManager::getProfile(const std::string& profileId) const {
    std::lock_guard<std::mutex> lock(m_impl->profilesMutex);
    
    auto it = std::find_if(m_impl->profiles.begin(), m_impl->profiles.end(),
        [&profileId](const Profile& p) { return p.id == profileId; });
    
    if (it != m_impl->profiles.end()) {
        return *it;
    }
    
    return std::nullopt;
}

std::optional<Profile> ProfileManager::getProfileByName(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_impl->profilesMutex);
    
    auto it = std::find_if(m_impl->profiles.begin(), m_impl->profiles.end(),
        [&name](const Profile& p) { return p.name == name; });
    
    if (it != m_impl->profiles.end()) {
        return *it;
    }
    
    return std::nullopt;
}

std::optional<Profile> ProfileManager::getActiveProfile() const {
    return getProfile(m_impl->activeProfileId);
}

bool ProfileManager::setActiveProfile(const std::string& profileId) {
    if (!getProfile(profileId)) return false;
    
    m_impl->activeProfileId = profileId;
    saveProfiles();
    
    if (m_impl->onActiveProfileChanged) {
        m_impl->onActiveProfileChanged(profileId);
    }
    
    return true;
}

std::string ProfileManager::getActiveProfileId() const {
    return m_impl->activeProfileId;
}

bool ProfileManager::validateProfile(const std::string& profileId) const {
    return getProfileIssues(profileId).empty();
}

std::vector<std::string> ProfileManager::getProfileIssues(const std::string& profileId) const {
    std::vector<std::string> issues;
    
    auto profile = getProfile(profileId);
    if (!profile) {
        issues.push_back("Profile not found");
        return issues;
    }
    
    if (profile->name.empty()) {
        issues.push_back("Profile name is empty");
    }
    
    if (profile->gameVersion.empty()) {
        issues.push_back("Game version not specified");
    }
    
    if (!std::filesystem::exists(profile->gameDirectory)) {
        issues.push_back("Game directory does not exist");
    }
    
    if (!profile->javaConfig.autoDetect && profile->javaConfig.path.empty()) {
        issues.push_back("Java path not configured");
    }
    
    if (profile->javaConfig.maxMemoryMB < profile->javaConfig.minMemoryMB) {
        issues.push_back("Max memory is less than min memory");
    }
    
    return issues;
}

std::filesystem::path ProfileManager::getProfileDirectory(const std::string& profileId) const {
    auto profile = getProfile(profileId);
    return profile ? std::filesystem::path(profile->gameDirectory) : std::filesystem::path();
}

std::filesystem::path ProfileManager::getModsDirectory(const std::string& profileId) const {
    return getProfileDirectory(profileId) / "mods";
}

std::filesystem::path ProfileManager::getSavesDirectory(const std::string& profileId) const {
    return getProfileDirectory(profileId) / "saves";
}

std::filesystem::path ProfileManager::getResourcePacksDirectory(const std::string& profileId) const {
    return getProfileDirectory(profileId) / "resourcepacks";
}

std::filesystem::path ProfileManager::getShaderPacksDirectory(const std::string& profileId) const {
    return getProfileDirectory(profileId) / "shaderpacks";
}

ProfileSnapshot ProfileManager::createSnapshot(const std::string& profileId, 
    const std::string& name, const std::string& description) {
    
    ProfileSnapshot snapshot;
    snapshot.id = generateProfileId();
    snapshot.name = name;
    snapshot.description = description;
    snapshot.createdAt = std::chrono::system_clock::now();
    
    auto profile = getProfile(profileId);
    if (!profile) return snapshot;
    
    // Create snapshot directory
    auto snapshotsDir = m_impl->profilesDirectory / profileId / "snapshots";
    std::filesystem::create_directories(snapshotsDir);
    
    snapshot.dataPath = snapshotsDir / snapshot.id;
    
    // Copy profile data
    std::error_code ec;
    std::filesystem::copy(profile->gameDirectory, snapshot.dataPath,
        std::filesystem::copy_options::recursive, ec);
    
    // Add snapshot to profile
    {
        std::lock_guard<std::mutex> lock(m_impl->profilesMutex);
        auto it = std::find_if(m_impl->profiles.begin(), m_impl->profiles.end(),
            [&profileId](const Profile& p) { return p.id == profileId; });
        if (it != m_impl->profiles.end()) {
            it->snapshots.push_back(snapshot);
            saveProfiles();
        }
    }
    
    core::Logger::instance().info( "Created snapshot: {} for profile {}", name, profileId);
    return snapshot;
}

bool ProfileManager::restoreSnapshot(const std::string& profileId, const std::string& snapshotId) {
    auto profile = getProfile(profileId);
    if (!profile) return false;
    
    auto it = std::find_if(profile->snapshots.begin(), profile->snapshots.end(),
        [&snapshotId](const ProfileSnapshot& s) { return s.id == snapshotId; });
    
    if (it == profile->snapshots.end()) return false;
    
    std::error_code ec;
    
    // Remove current data
    std::filesystem::remove_all(profile->gameDirectory, ec);
    
    // Restore from snapshot
    std::filesystem::copy(it->dataPath, profile->gameDirectory,
        std::filesystem::copy_options::recursive, ec);
    
    core::Logger::instance().info( "Restored snapshot: {} for profile {}", it->name, profileId);
    return !ec;
}

bool ProfileManager::deleteSnapshot(const std::string& profileId, const std::string& snapshotId) {
    std::lock_guard<std::mutex> lock(m_impl->profilesMutex);
    
    auto it = std::find_if(m_impl->profiles.begin(), m_impl->profiles.end(),
        [&profileId](const Profile& p) { return p.id == profileId; });
    
    if (it == m_impl->profiles.end()) return false;
    
    auto snapIt = std::find_if(it->snapshots.begin(), it->snapshots.end(),
        [&snapshotId](const ProfileSnapshot& s) { return s.id == snapshotId; });
    
    if (snapIt == it->snapshots.end()) return false;
    
    std::error_code ec;
    std::filesystem::remove_all(snapIt->dataPath, ec);
    
    it->snapshots.erase(snapIt);
    saveProfiles();
    
    return true;
}

std::vector<ProfileSnapshot> ProfileManager::getSnapshots(const std::string& profileId) const {
    auto profile = getProfile(profileId);
    return profile ? profile->snapshots : std::vector<ProfileSnapshot>{};
}

std::vector<JavaConfig> ProfileManager::detectInstalledJava() {
    std::vector<JavaConfig> javaInstalls;
    
    std::vector<std::filesystem::path> searchPaths;
    
#ifdef _WIN32
    searchPaths.push_back("C:/Program Files/Java");
    searchPaths.push_back("C:/Program Files (x86)/Java");
    searchPaths.push_back("C:/Program Files/Eclipse Adoptium");
    searchPaths.push_back("C:/Program Files/Zulu");
    
    // Check environment variable
    if (auto javaHome = std::getenv("JAVA_HOME")) {
        searchPaths.push_back(javaHome);
    }
#elif __APPLE__
    searchPaths.push_back("/Library/Java/JavaVirtualMachines");
    searchPaths.push_back("/usr/local/opt/openjdk");
    searchPaths.push_back("/opt/homebrew/opt/openjdk");
#else
    searchPaths.push_back("/usr/lib/jvm");
    searchPaths.push_back("/usr/local/lib/jvm");
    searchPaths.push_back("/opt/java");
#endif
    
    for (const auto& searchPath : searchPaths) {
        if (!std::filesystem::exists(searchPath)) continue;
        
        for (const auto& entry : std::filesystem::directory_iterator(searchPath)) {
            if (entry.is_directory()) {
                auto javaBin = entry.path() / "bin" / 
#ifdef _WIN32
                    "java.exe";
#else
                    "java";
#endif
                
                if (std::filesystem::exists(javaBin)) {
                    JavaConfig config;
                    config.path = javaBin.string();
                    config.version = entry.path().filename().string();
                    config.autoDetect = false;
                    javaInstalls.push_back(config);
                }
            }
        }
    }
    
    core::Logger::instance().info( "Detected {} Java installations", javaInstalls.size());
    return javaInstalls;
}

void ProfileManager::updatePlayTime(const std::string& profileId, int64_t sessionSeconds) {
    std::lock_guard<std::mutex> lock(m_impl->profilesMutex);
    
    auto it = std::find_if(m_impl->profiles.begin(), m_impl->profiles.end(),
        [&profileId](const Profile& p) { return p.id == profileId; });
    
    if (it != m_impl->profiles.end()) {
        it->totalPlayTime += sessionSeconds;
        it->lastPlayed = std::chrono::system_clock::now();
        saveProfiles();
    }
}

int64_t ProfileManager::getTotalPlayTime(const std::string& profileId) const {
    auto profile = getProfile(profileId);
    return profile ? profile->totalPlayTime : 0;
}

std::chrono::system_clock::time_point ProfileManager::getLastPlayed(const std::string& profileId) const {
    auto profile = getProfile(profileId);
    return profile ? profile->lastPlayed : std::chrono::system_clock::time_point{};
}

void ProfileManager::setOnProfileCreated(std::function<void(const Profile&)> callback) {
    m_impl->onProfileCreated = std::move(callback);
}

void ProfileManager::setOnProfileUpdated(std::function<void(const Profile&)> callback) {
    m_impl->onProfileUpdated = std::move(callback);
}

void ProfileManager::setOnProfileDeleted(std::function<void(const std::string&)> callback) {
    m_impl->onProfileDeleted = std::move(callback);
}

void ProfileManager::setOnActiveProfileChanged(std::function<void(const std::string&)> callback) {
    m_impl->onActiveProfileChanged = std::move(callback);
}

void ProfileManager::setProfilesDirectory(const std::filesystem::path& path) {
    m_impl->profilesDirectory = path;
}

std::filesystem::path ProfileManager::getProfilesDirectory() const {
    return m_impl->profilesDirectory;
}

void ProfileManager::saveProfiles() {
    auto profilesFile = m_impl->profilesDirectory / "profiles.json";
    
    nlohmann::json j;
    j["activeProfileId"] = m_impl->activeProfileId;
    
    nlohmann::json profilesArray = nlohmann::json::array();
    for (const auto& profile : m_impl->profiles) {
        profilesArray.push_back(profile.toJson());
    }
    j["profiles"] = profilesArray;
    
    std::ofstream file(profilesFile);
    if (file) {
        file << j.dump(2);
    }
}

void ProfileManager::loadProfiles() {
    auto profilesFile = m_impl->profilesDirectory / "profiles.json";
    
    if (!std::filesystem::exists(profilesFile)) return;
    
    std::ifstream file(profilesFile);
    if (!file) return;
    
    try {
        nlohmann::json j;
        file >> j;
        
        m_impl->activeProfileId = j.value("activeProfileId", "");
        
        if (j.contains("profiles") && j["profiles"].is_array()) {
            for (const auto& p : j["profiles"]) {
                m_impl->profiles.push_back(Profile::fromJson(p));
            }
        }
    } catch (const std::exception& e) {
        core::Logger::instance().error( "Failed to load profiles: {}", e.what());
    }
}

std::string ProfileManager::generateProfileId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    const char* hex = "0123456789abcdef";
    std::string id;
    id.reserve(32);
    
    for (int i = 0; i < 32; ++i) {
        id += hex[dis(gen)];
    }
    
    return id;
}

// ProfileBuilder implementation
ProfileBuilder::ProfileBuilder(const std::string& name, const std::string& gameVersion) {
    m_profile.name = name;
    m_profile.gameVersion = gameVersion;
    m_profile.createdAt = std::chrono::system_clock::now();
    m_profile.loader.type = "vanilla";
}

ProfileBuilder& ProfileBuilder::withIcon(const std::string& iconPath) {
    m_profile.iconPath = iconPath;
    return *this;
}

ProfileBuilder& ProfileBuilder::withJava(const JavaConfig& config) {
    m_profile.javaConfig = config;
    return *this;
}

ProfileBuilder& ProfileBuilder::withResolution(int width, int height, bool fullscreen) {
    m_profile.resolution.width = width;
    m_profile.resolution.height = height;
    m_profile.resolution.fullscreen = fullscreen;
    return *this;
}

ProfileBuilder& ProfileBuilder::withLoader(const std::string& type, const std::string& version) {
    m_profile.loader.type = type;
    m_profile.loader.version = version;
    return *this;
}

ProfileBuilder& ProfileBuilder::withMods(const std::vector<std::string>& mods) {
    m_profile.enabledMods = mods;
    return *this;
}

ProfileBuilder& ProfileBuilder::withGameDirectory(const std::string& path) {
    m_profile.gameDirectory = path;
    return *this;
}

ProfileBuilder& ProfileBuilder::asQuickLaunch(bool enabled) {
    m_profile.quickLaunch = enabled;
    return *this;
}

Profile ProfileBuilder::build() {
    return m_profile;
}

} // namespace konami::profile
