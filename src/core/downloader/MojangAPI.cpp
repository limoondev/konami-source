/**
 * MojangAPI.cpp
 * 
 * Interface to Mojang's official Minecraft APIs.
 */

#include "MojangAPI.hpp"
#include "../Logger.hpp"

#include <cpr/cpr.h>
#include <algorithm>
#include <sstream>

namespace konami::core::downloader {

using json = nlohmann::json;

std::future<std::vector<VersionInfo>> MojangAPI::getVersionManifest() {
    return std::async(std::launch::async, [this]() -> std::vector<VersionInfo> {
        try {
            cpr::Response response = cpr::Get(
                cpr::Url{VERSION_MANIFEST_URL},
                cpr::Timeout{30000}
            );

            if (response.status_code != 200) {
                Logger::instance().error("Failed to fetch version manifest: HTTP {}", response.status_code);
                return {};
            }

            auto j = json::parse(response.text);

            m_latestRelease = j["latest"]["release"].get<std::string>();
            m_latestSnapshot = j["latest"]["snapshot"].get<std::string>();

            std::vector<VersionInfo> versions;
            for (const auto& v : j["versions"]) {
                versions.push_back(parseVersionInfo(v));
            }

            m_cachedManifest = versions;
            Logger::instance().info("Fetched {} versions from Mojang", versions.size());
            return versions;

        } catch (const std::exception& e) {
            Logger::instance().error("Version manifest error: {}", e.what());
            return m_cachedManifest;
        }
    });
}

std::future<std::optional<VersionInfo>> MojangAPI::getLatestRelease() {
    return std::async(std::launch::async, [this]() -> std::optional<VersionInfo> {
        if (m_cachedManifest.empty()) {
            auto future = getVersionManifest();
            future.get();
        }

        for (const auto& v : m_cachedManifest) {
            if (v.id == m_latestRelease) {
                return v;
            }
        }
        return std::nullopt;
    });
}

std::future<std::optional<VersionInfo>> MojangAPI::getLatestSnapshot() {
    return std::async(std::launch::async, [this]() -> std::optional<VersionInfo> {
        if (m_cachedManifest.empty()) {
            auto future = getVersionManifest();
            future.get();
        }

        for (const auto& v : m_cachedManifest) {
            if (v.id == m_latestSnapshot) {
                return v;
            }
        }
        return std::nullopt;
    });
}

std::future<std::optional<VersionData>> MojangAPI::getVersionData(const VersionInfo& versionInfo) {
    return std::async(std::launch::async, [this, versionInfo]() -> std::optional<VersionData> {
        try {
            cpr::Response response = cpr::Get(
                cpr::Url{versionInfo.url},
                cpr::Timeout{30000}
            );

            if (response.status_code != 200) {
                Logger::instance().error("Failed to fetch version data for {}: HTTP {}",
                    versionInfo.id, response.status_code);
                return std::nullopt;
            }

            auto j = json::parse(response.text);
            return parseVersionData(j);

        } catch (const std::exception& e) {
            Logger::instance().error("Version data error: {}", e.what());
            return std::nullopt;
        }
    });
}

std::future<std::optional<VersionData>> MojangAPI::getVersionDataById(const std::string& versionId) {
    return std::async(std::launch::async, [this, versionId]() -> std::optional<VersionData> {
        if (m_cachedManifest.empty()) {
            auto future = getVersionManifest();
            future.get();
        }

        for (const auto& v : m_cachedManifest) {
            if (v.id == versionId) {
                auto future = getVersionData(v);
                return future.get();
            }
        }

        Logger::instance().warn("Version not found: {}", versionId);
        return std::nullopt;
    });
}

std::future<std::vector<AssetObject>> MojangAPI::getAssetIndex(const AssetIndex& assetIndex) {
    return std::async(std::launch::async, [this, assetIndex]() -> std::vector<AssetObject> {
        try {
            cpr::Response response = cpr::Get(
                cpr::Url{assetIndex.url},
                cpr::Timeout{30000}
            );

            if (response.status_code != 200) {
                Logger::instance().error("Failed to fetch asset index: HTTP {}", response.status_code);
                return {};
            }

            auto j = json::parse(response.text);
            std::vector<AssetObject> assets;

            if (j.contains("objects")) {
                for (auto& [name, obj] : j["objects"].items()) {
                    assets.push_back(parseAssetObject(name, obj));
                }
            }

            Logger::instance().info("Fetched {} assets", assets.size());
            return assets;

        } catch (const std::exception& e) {
            Logger::instance().error("Asset index error: {}", e.what());
            return {};
        }
    });
}

std::string MojangAPI::getAssetUrl(const AssetObject& asset) {
    if (asset.hash.size() < 2) return "";
    return std::string(RESOURCES_URL) + "/" + asset.hash.substr(0, 2) + "/" + asset.hash;
}

std::string MojangAPI::getLibraryUrl(const LibraryInfo& library) {
    if (!library.download.url.empty()) {
        return library.download.url;
    }
    return std::string(LIBRARIES_URL) + "/" + getLibraryPath(library.name);
}

std::string MojangAPI::getLibraryPath(const std::string& name) {
    // Maven coordinate: group:artifact:version
    std::istringstream iss(name);
    std::string group, artifact, version;

    std::getline(iss, group, ':');
    std::getline(iss, artifact, ':');
    std::getline(iss, version, ':');

    // Convert dots to slashes in group
    std::replace(group.begin(), group.end(), '.', '/');

    return group + "/" + artifact + "/" + version + "/" + artifact + "-" + version + ".jar";
}

bool MojangAPI::libraryAppliesToPlatform(const LibraryInfo& library) {
    if (library.rules.empty()) return true;

    for (const auto& rule : library.rules) {
#ifdef _WIN32
        if (rule == "windows") return true;
#elif defined(__APPLE__)
        if (rule == "osx" || rule == "macos") return true;
#else
        if (rule == "linux") return true;
#endif
    }

    return false;
}

VersionType MojangAPI::parseVersionType(const std::string& type) {
    if (type == "release") return VersionType::Release;
    if (type == "snapshot") return VersionType::Snapshot;
    if (type == "old_beta") return VersionType::OldBeta;
    if (type == "old_alpha") return VersionType::OldAlpha;
    return VersionType::Release;
}

VersionInfo MojangAPI::parseVersionInfo(const nlohmann::json& j) {
    VersionInfo info;
    info.id = j.value("id", "");
    info.type = parseVersionType(j.value("type", "release"));
    info.url = j.value("url", "");
    info.time = j.value("time", "");
    info.releaseTime = j.value("releaseTime", "");
    info.sha1 = j.value("sha1", "");
    info.complianceLevel = j.value("complianceLevel", 0);
    return info;
}

VersionData MojangAPI::parseVersionData(const nlohmann::json& j) {
    VersionData data;
    data.id = j.value("id", "");
    data.type = parseVersionType(j.value("type", "release"));
    data.mainClass = j.value("mainClass", "");
    data.minecraftArguments = j.value("minecraftArguments", "");
    data.rawJson = j;

    if (j.contains("javaVersion")) {
        data.javaVersion = j["javaVersion"].value("component", "");
        data.javaVersionMajor = j["javaVersion"].value("majorVersion", 8);
    }

    if (j.contains("assetIndex")) {
        auto& ai = j["assetIndex"];
        data.assetIndex.id = ai.value("id", "");
        data.assetIndex.url = ai.value("url", "");
        data.assetIndex.sha1 = ai.value("sha1", "");
        data.assetIndex.size = ai.value("size", size_t(0));
        data.assetIndex.totalSize = ai.value("totalSize", size_t(0));
    }

    if (j.contains("downloads")) {
        if (j["downloads"].contains("client")) {
            auto& c = j["downloads"]["client"];
            data.clientDownload.url = c.value("url", "");
            data.clientDownload.sha1 = c.value("sha1", "");
            data.clientDownload.size = c.value("size", size_t(0));
        }
        if (j["downloads"].contains("server")) {
            auto& s = j["downloads"]["server"];
            data.serverDownload.url = s.value("url", "");
            data.serverDownload.sha1 = s.value("sha1", "");
            data.serverDownload.size = s.value("size", size_t(0));
        }
    }

    if (j.contains("libraries")) {
        for (const auto& lib : j["libraries"]) {
            data.libraries.push_back(parseLibrary(lib));
        }
    }

    return data;
}

LibraryInfo MojangAPI::parseLibrary(const nlohmann::json& j) {
    LibraryInfo lib;
    lib.name = j.value("name", "");

    if (j.contains("downloads") && j["downloads"].contains("artifact")) {
        auto& artifact = j["downloads"]["artifact"];
        lib.download.url = artifact.value("url", "");
        lib.download.sha1 = artifact.value("sha1", "");
        lib.download.size = artifact.value("size", size_t(0));
    }

    if (j.contains("natives")) {
#ifdef _WIN32
        if (j["natives"].contains("windows")) {
            lib.isNative = true;
            lib.nativesClassifier = j["natives"]["windows"].get<std::string>();
        }
#elif defined(__APPLE__)
        if (j["natives"].contains("osx")) {
            lib.isNative = true;
            lib.nativesClassifier = j["natives"]["osx"].get<std::string>();
        }
#else
        if (j["natives"].contains("linux")) {
            lib.isNative = true;
            lib.nativesClassifier = j["natives"]["linux"].get<std::string>();
        }
#endif
    }

    if (j.contains("rules")) {
        for (const auto& rule : j["rules"]) {
            if (rule.contains("os") && rule["os"].contains("name")) {
                lib.rules.push_back(rule["os"]["name"].get<std::string>());
            }
        }
    }

    return lib;
}

AssetObject MojangAPI::parseAssetObject(const std::string& name, const nlohmann::json& j) {
    AssetObject obj;
    obj.name = name;
    obj.hash = j.value("hash", "");
    obj.size = j.value("size", size_t(0));
    return obj;
}

} // namespace konami::core::downloader
