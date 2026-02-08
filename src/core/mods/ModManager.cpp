#include "ModManager.hpp"
#include "../Logger.hpp"
#include "../downloader/DownloadManager.hpp"
#include <zip.h>
#include <fstream>
#include <regex>
#include <algorithm>

namespace konami::mods {

// JSON serialization
nlohmann::json ModInfo::toJson() const {
    return {
        {"id", id},
        {"name", name},
        {"version", version},
        {"description", description},
        {"author", author},
        {"website", website},
        {"iconPath", iconPath},
        {"filePath", filePath},
        {"sha256Hash", sha256Hash},
        {"loader", modLoaderToString(loader)},
        {"source", modSourceToString(source)},
        {"gameVersions", gameVersions},
        {"categories", categories},
        {"fileSize", fileSize},
        {"downloads", downloads},
        {"enabled", enabled},
        {"hasUpdate", hasUpdate},
        {"latestVersion", latestVersion}
    };
}

ModInfo ModInfo::fromJson(const nlohmann::json& j) {
    ModInfo info;
    info.id = j.value("id", "");
    info.name = j.value("name", "");
    info.version = j.value("version", "");
    info.description = j.value("description", "");
    info.author = j.value("author", "");
    info.website = j.value("website", "");
    info.iconPath = j.value("iconPath", "");
    info.filePath = j.value("filePath", "");
    info.sha256Hash = j.value("sha256Hash", "");
    info.loader = stringToModLoader(j.value("loader", "none"));
    info.source = stringToModSource(j.value("source", "local"));
    info.gameVersions = j.value("gameVersions", std::vector<std::string>{});
    info.categories = j.value("categories", std::vector<std::string>{});
    info.fileSize = j.value("fileSize", 0);
    info.downloads = j.value("downloads", 0);
    info.enabled = j.value("enabled", true);
    info.hasUpdate = j.value("hasUpdate", false);
    info.latestVersion = j.value("latestVersion", "");
    return info;
}

// ModManager Implementation
class ModManager::Impl {
public:
    std::filesystem::path modsDirectory;
    std::filesystem::path cacheDirectory;
    std::vector<ModInfo> installedMods;
    std::unordered_map<std::string, std::vector<ModDependency>> dependencyCache;
    std::vector<ModConflict> detectedConflicts;
    
    std::unique_ptr<CurseForgeClient> curseForgeClient;
    std::unique_ptr<ModrinthClient> modrinthClient;
    
    // Callbacks
    std::function<void(const ModInfo&)> onModInstalled;
    std::function<void(const std::string&)> onModRemoved;
    std::function<void(const ModInfo&)> onModUpdated;
    std::function<void(const ModConflict&)> onConflictDetected;
    
    std::mutex modsMutex;
    
    bool parseForgeModInfo(const std::filesystem::path& jarPath, ModInfo& info) {
        int err;
        zip_t* archive = zip_open(jarPath.string().c_str(), ZIP_RDONLY, &err);
        if (!archive) return false;
        
        // Try mcmod.info (legacy Forge)
        zip_stat_t stat;
        if (zip_stat(archive, "mcmod.info", 0, &stat) == 0) {
            zip_file_t* file = zip_fopen(archive, "mcmod.info", 0);
            if (file) {
                std::string content(stat.size, '\0');
                zip_fread(file, content.data(), stat.size);
                zip_fclose(file);
                
                try {
                    auto json = nlohmann::json::parse(content);
                    if (json.is_array() && !json.empty()) {
                        auto& mod = json[0];
                        info.id = mod.value("modid", "");
                        info.name = mod.value("name", "");
                        info.version = mod.value("version", "");
                        info.description = mod.value("description", "");
                        info.author = mod.value("authorList", std::vector<std::string>{}).empty() 
                            ? "" : mod["authorList"][0].get<std::string>();
                        info.loader = ModLoader::Forge;
                        zip_close(archive);
                        return true;
                    }
                } catch (...) {}
            }
        }
        
        // Try mods.toml (modern Forge/NeoForge)
        if (zip_stat(archive, "META-INF/mods.toml", 0, &stat) == 0) {
            zip_file_t* file = zip_fopen(archive, "META-INF/mods.toml", 0);
            if (file) {
                std::string content(stat.size, '\0');
                zip_fread(file, content.data(), stat.size);
                zip_fclose(file);
                
                // Simple TOML parsing for mod info
                std::regex modIdRegex(R"(modId\s*=\s*"([^"]+)")");
                std::regex versionRegex(R"(version\s*=\s*"([^"]+)")");
                std::regex displayNameRegex(R"(displayName\s*=\s*"([^"]+)")");
                
                std::smatch match;
                if (std::regex_search(content, match, modIdRegex)) {
                    info.id = match[1];
                }
                if (std::regex_search(content, match, versionRegex)) {
                    info.version = match[1];
                }
                if (std::regex_search(content, match, displayNameRegex)) {
                    info.name = match[1];
                }
                
                info.loader = ModLoader::Forge;
                zip_close(archive);
                return !info.id.empty();
            }
        }
        
        zip_close(archive);
        return false;
    }
    
    bool parseFabricModInfo(const std::filesystem::path& jarPath, ModInfo& info) {
        int err;
        zip_t* archive = zip_open(jarPath.string().c_str(), ZIP_RDONLY, &err);
        if (!archive) return false;
        
        zip_stat_t stat;
        if (zip_stat(archive, "fabric.mod.json", 0, &stat) == 0) {
            zip_file_t* file = zip_fopen(archive, "fabric.mod.json", 0);
            if (file) {
                std::string content(stat.size, '\0');
                zip_fread(file, content.data(), stat.size);
                zip_fclose(file);
                zip_close(archive);
                
                try {
                    auto json = nlohmann::json::parse(content);
                    info.id = json.value("id", "");
                    info.name = json.value("name", "");
                    info.version = json.value("version", "");
                    info.description = json.value("description", "");
                    
                    if (json.contains("authors") && json["authors"].is_array() && !json["authors"].empty()) {
                        auto& author = json["authors"][0];
                        if (author.is_string()) {
                            info.author = author.get<std::string>();
                        } else if (author.is_object()) {
                            info.author = author.value("name", "");
                        }
                    }
                    
                    if (json.contains("contact")) {
                        info.website = json["contact"].value("homepage", 
                            json["contact"].value("sources", ""));
                    }
                    
                    if (json.contains("icon")) {
                        info.iconPath = json["icon"].get<std::string>();
                    }
                    
                    info.loader = ModLoader::Fabric;
                    return true;
                } catch (...) {}
            }
        }
        
        zip_close(archive);
        return false;
    }
    
    bool parseQuiltModInfo(const std::filesystem::path& jarPath, ModInfo& info) {
        int err;
        zip_t* archive = zip_open(jarPath.string().c_str(), ZIP_RDONLY, &err);
        if (!archive) return false;
        
        zip_stat_t stat;
        if (zip_stat(archive, "quilt.mod.json", 0, &stat) == 0) {
            zip_file_t* file = zip_fopen(archive, "quilt.mod.json", 0);
            if (file) {
                std::string content(stat.size, '\0');
                zip_fread(file, content.data(), stat.size);
                zip_fclose(file);
                zip_close(archive);
                
                try {
                    auto json = nlohmann::json::parse(content);
                    auto& loader = json["quilt_loader"];
                    info.id = loader.value("id", "");
                    info.version = loader.value("version", "");
                    
                    if (loader.contains("metadata")) {
                        auto& meta = loader["metadata"];
                        info.name = meta.value("name", info.id);
                        info.description = meta.value("description", "");
                    }
                    
                    info.loader = ModLoader::Quilt;
                    return true;
                } catch (...) {}
            }
        }
        
        zip_close(archive);
        return false;
    }
    
    std::string calculateSha256(const std::filesystem::path& filePath) {
        // SHA256 calculation using OpenSSL
        std::ifstream file(filePath, std::ios::binary);
        if (!file) return "";
        
        unsigned char hash[32];
        // Placeholder - would use EVP_Digest in real implementation
        
        std::stringstream ss;
        for (int i = 0; i < 32; i++) {
            ss << std::hex << std::setfill('0') << std::setw(2) << (int)hash[i];
        }
        return ss.str();
    }
};

ModManager::ModManager() : m_impl(std::make_unique<Impl>()) {
    m_impl->curseForgeClient = std::make_unique<CurseForgeClient>("");
    m_impl->modrinthClient = std::make_unique<ModrinthClient>();
}

ModManager::~ModManager() = default;

bool ModManager::initialize(const std::filesystem::path& modsDirectory) {
    m_impl->modsDirectory = modsDirectory;
    
    if (!std::filesystem::exists(modsDirectory)) {
        std::filesystem::create_directories(modsDirectory);
    }
    
    m_impl->cacheDirectory = modsDirectory.parent_path() / "mod_cache";
    if (!std::filesystem::exists(m_impl->cacheDirectory)) {
        std::filesystem::create_directories(m_impl->cacheDirectory);
    }
    
    core::Logger::instance().info("Initialized ModManager with directory: {}", modsDirectory.string());
    return refreshModList();
}

void ModManager::shutdown() {
    std::lock_guard<std::mutex> lock(m_impl->modsMutex);
    m_impl->installedMods.clear();
    m_impl->dependencyCache.clear();
    m_impl->detectedConflicts.clear();
}

std::vector<ModInfo> ModManager::scanInstalledMods() {
    std::vector<ModInfo> mods;
    
    if (!std::filesystem::exists(m_impl->modsDirectory)) {
        return mods;
    }
    
    for (const auto& entry : std::filesystem::directory_iterator(m_impl->modsDirectory)) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            
            if (ext == ".jar" || ext == ".disabled") {
                auto modInfo = parseModFile(entry.path());
                if (modInfo) {
                    modInfo->enabled = (ext == ".jar");
                    mods.push_back(*modInfo);
                }
            }
        }
    }
    
    return mods;
}

std::optional<ModInfo> ModManager::parseModFile(const std::filesystem::path& modPath) {
    ModInfo info;
    info.filePath = modPath.string();
    info.fileSize = std::filesystem::file_size(modPath);
    info.source = ModSource::Local;
    
    // Try different mod formats
    if (m_impl->parseFabricModInfo(modPath, info)) {
        return info;
    }
    if (m_impl->parseQuiltModInfo(modPath, info)) {
        return info;
    }
    if (m_impl->parseForgeModInfo(modPath, info)) {
        return info;
    }
    
    // Fallback: use filename
    info.name = modPath.stem().string();
    info.id = info.name;
    info.loader = ModLoader::None;
    
    return info;
}

bool ModManager::refreshModList() {
    std::lock_guard<std::mutex> lock(m_impl->modsMutex);
    m_impl->installedMods = scanInstalledMods();
    m_impl->detectedConflicts = detectConflicts();
    
    core::Logger::instance().info("Found {} installed mods", m_impl->installedMods.size());
    return true;
}

bool ModManager::enableMod(const std::string& modId) {
    std::lock_guard<std::mutex> lock(m_impl->modsMutex);
    
    for (auto& mod : m_impl->installedMods) {
        if (mod.id == modId && !mod.enabled) {
            std::filesystem::path currentPath(mod.filePath);
            std::filesystem::path newPath = currentPath;
            newPath.replace_extension(".jar");
            
            try {
                std::filesystem::rename(currentPath, newPath);
                mod.filePath = newPath.string();
                mod.enabled = true;
                core::Logger::instance().info("Enabled mod: {}", mod.name);
                return true;
            } catch (const std::exception& e) {
                core::Logger::instance().error("Failed to enable mod {}: {}", modId, e.what());
                return false;
            }
        }
    }
    
    return false;
}

bool ModManager::disableMod(const std::string& modId) {
    std::lock_guard<std::mutex> lock(m_impl->modsMutex);
    
    for (auto& mod : m_impl->installedMods) {
        if (mod.id == modId && mod.enabled) {
            std::filesystem::path currentPath(mod.filePath);
            std::filesystem::path newPath = currentPath;
            newPath.replace_extension(".disabled");
            
            try {
                std::filesystem::rename(currentPath, newPath);
                mod.filePath = newPath.string();
                mod.enabled = false;
                core::Logger::instance().info("Disabled mod: {}", mod.name);
                return true;
            } catch (const std::exception& e) {
                core::Logger::instance().error("Failed to disable mod {}: {}", modId, e.what());
                return false;
            }
        }
    }
    
    return false;
}

bool ModManager::deleteMod(const std::string& modId) {
    std::lock_guard<std::mutex> lock(m_impl->modsMutex);
    
    auto it = std::find_if(m_impl->installedMods.begin(), m_impl->installedMods.end(),
        [&modId](const ModInfo& mod) { return mod.id == modId; });
    
    if (it != m_impl->installedMods.end()) {
        try {
            std::filesystem::remove(it->filePath);
            
            if (m_impl->onModRemoved) {
                m_impl->onModRemoved(modId);
            }
            
            m_impl->installedMods.erase(it);
            core::Logger::instance().info("Deleted mod: {}", modId);
            return true;
        } catch (const std::exception& e) {
            core::Logger::instance().error("Failed to delete mod {}: {}", modId, e.what());
            return false;
        }
    }
    
    return false;
}

std::future<bool> ModManager::installMod(const ModInfo& mod, DownloadProgressCallback progressCallback) {
    return std::async(std::launch::async, [this, mod, progressCallback]() {
        // Implementation would download from source
        core::Logger::instance().info("Installing mod: {}", mod.name);
        
        // Simulate installation
        if (progressCallback) {
            for (int i = 0; i <= 100; i += 10) {
                progressCallback(mod.id, i / 100.0, i * 1024, 100 * 1024);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        
        std::lock_guard<std::mutex> lock(m_impl->modsMutex);
        m_impl->installedMods.push_back(mod);
        
        if (m_impl->onModInstalled) {
            m_impl->onModInstalled(mod);
        }
        
        return true;
    });
}

std::vector<ModConflict> ModManager::detectConflicts() {
    std::vector<ModConflict> conflicts;
    
    // Check for duplicate mod IDs
    std::unordered_map<std::string, std::vector<size_t>> idMap;
    for (size_t i = 0; i < m_impl->installedMods.size(); ++i) {
        idMap[m_impl->installedMods[i].id].push_back(i);
    }
    
    for (const auto& [id, indices] : idMap) {
        if (indices.size() > 1) {
            for (size_t i = 1; i < indices.size(); ++i) {
                ModConflict conflict;
                conflict.modId1 = m_impl->installedMods[indices[0]].name;
                conflict.modId2 = m_impl->installedMods[indices[i]].name;
                conflict.reason = "Duplicate mod ID: " + id;
                conflict.severity = ModConflict::Severity::Error;
                conflicts.push_back(conflict);
            }
        }
    }
    
    // Check for loader incompatibilities
    bool hasForge = false, hasFabric = false, hasQuilt = false;
    for (const auto& mod : m_impl->installedMods) {
        if (!mod.enabled) continue;
        if (mod.loader == ModLoader::Forge || mod.loader == ModLoader::NeoForge) hasForge = true;
        if (mod.loader == ModLoader::Fabric) hasFabric = true;
        if (mod.loader == ModLoader::Quilt) hasQuilt = true;
    }
    
    if (hasForge && hasFabric) {
        ModConflict conflict;
        conflict.modId1 = "Forge Mods";
        conflict.modId2 = "Fabric Mods";
        conflict.reason = "Forge and Fabric mods cannot be used together";
        conflict.severity = ModConflict::Severity::Critical;
        conflicts.push_back(conflict);
    }
    
    return conflicts;
}

bool ModManager::hasConflicts() const {
    return !m_impl->detectedConflicts.empty();
}

std::vector<ModInfo> ModManager::getInstalledMods() const {
    std::lock_guard<std::mutex> lock(m_impl->modsMutex);
    return m_impl->installedMods;
}

std::optional<ModInfo> ModManager::getModInfo(const std::string& modId) const {
    std::lock_guard<std::mutex> lock(m_impl->modsMutex);
    
    auto it = std::find_if(m_impl->installedMods.begin(), m_impl->installedMods.end(),
        [&modId](const ModInfo& mod) { return mod.id == modId; });
    
    if (it != m_impl->installedMods.end()) {
        return *it;
    }
    
    return std::nullopt;
}

bool ModManager::isModInstalled(const std::string& modId) const {
    return getModInfo(modId).has_value();
}

bool ModManager::isModEnabled(const std::string& modId) const {
    auto info = getModInfo(modId);
    return info && info->enabled;
}

void ModManager::setModsDirectory(const std::filesystem::path& path) {
    m_impl->modsDirectory = path;
}

std::filesystem::path ModManager::getModsDirectory() const {
    return m_impl->modsDirectory;
}

void ModManager::setCacheDirectory(const std::filesystem::path& path) {
    m_impl->cacheDirectory = path;
}

bool ModManager::exportModList(const std::filesystem::path& outputPath) {
    nlohmann::json exportJson;
    exportJson["formatVersion"] = 1;
    exportJson["name"] = "Konami Client Mod Export";
    
    nlohmann::json modsArray = nlohmann::json::array();
    for (const auto& mod : m_impl->installedMods) {
        modsArray.push_back(mod.toJson());
    }
    exportJson["mods"] = modsArray;
    
    std::ofstream file(outputPath);
    if (!file) return false;
    
    file << exportJson.dump(2);
    return true;
}

void ModManager::setOnModInstalled(std::function<void(const ModInfo&)> callback) {
    m_impl->onModInstalled = std::move(callback);
}

void ModManager::setOnModRemoved(std::function<void(const std::string&)> callback) {
    m_impl->onModRemoved = std::move(callback);
}

void ModManager::setOnModUpdated(std::function<void(const ModInfo&)> callback) {
    m_impl->onModUpdated = std::move(callback);
}

void ModManager::setOnConflictDetected(std::function<void(const ModConflict&)> callback) {
    m_impl->onConflictDetected = std::move(callback);
}

// Utility functions
std::string modLoaderToString(ModLoader loader) {
    switch (loader) {
        case ModLoader::Forge: return "forge";
        case ModLoader::Fabric: return "fabric";
        case ModLoader::Quilt: return "quilt";
        case ModLoader::LiteLoader: return "liteloader";
        case ModLoader::NeoForge: return "neoforge";
        default: return "none";
    }
}

ModLoader stringToModLoader(const std::string& str) {
    if (str == "forge") return ModLoader::Forge;
    if (str == "fabric") return ModLoader::Fabric;
    if (str == "quilt") return ModLoader::Quilt;
    if (str == "liteloader") return ModLoader::LiteLoader;
    if (str == "neoforge") return ModLoader::NeoForge;
    return ModLoader::None;
}

std::string modSourceToString(ModSource source) {
    switch (source) {
        case ModSource::CurseForge: return "curseforge";
        case ModSource::Modrinth: return "modrinth";
        case ModSource::GitHub: return "github";
        case ModSource::Custom: return "custom";
        default: return "local";
    }
}

ModSource stringToModSource(const std::string& str) {
    if (str == "curseforge") return ModSource::CurseForge;
    if (str == "modrinth") return ModSource::Modrinth;
    if (str == "github") return ModSource::GitHub;
    if (str == "custom") return ModSource::Custom;
    return ModSource::Local;
}

// CurseForge Client Implementation
class CurseForgeClient::Impl {
public:
    std::string apiKey;
    std::string baseUrl = "https://api.curseforge.com/v1";
};

CurseForgeClient::CurseForgeClient(const std::string& apiKey) 
    : m_impl(std::make_unique<Impl>()) {
    m_impl->apiKey = apiKey;
}

CurseForgeClient::~CurseForgeClient() = default;

std::future<ModSearchResult> CurseForgeClient::search(const ModSearchFilter& filter) {
    return std::async(std::launch::async, [this, filter]() {
        ModSearchResult result;
        // HTTP request to CurseForge API would go here
        return result;
    });
}

void CurseForgeClient::setApiKey(const std::string& key) {
    m_impl->apiKey = key;
}

bool CurseForgeClient::isApiKeyValid() const {
    return !m_impl->apiKey.empty();
}

// Modrinth Client Implementation
class ModrinthClient::Impl {
public:
    std::string userAgent = "KonamiClient/1.0.0";
    std::string baseUrl = "https://api.modrinth.com/v2";
};

ModrinthClient::ModrinthClient() : m_impl(std::make_unique<Impl>()) {}

ModrinthClient::~ModrinthClient() = default;

std::future<ModSearchResult> ModrinthClient::search(const ModSearchFilter& filter) {
    return std::async(std::launch::async, [this, filter]() {
        ModSearchResult result;
        // HTTP request to Modrinth API would go here
        return result;
    });
}

void ModrinthClient::setUserAgent(const std::string& userAgent) {
    m_impl->userAgent = userAgent;
}

} // namespace konami::mods
