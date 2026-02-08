#pragma once

#include <filesystem>
#include <string>
#include <cstdlib>

namespace konami::utils {

namespace fs = std::filesystem;

class PathUtils {
public:
    static fs::path getAppDataPath() {
#ifdef _WIN32
        const char* appData = std::getenv("APPDATA");
        return appData ? fs::path(appData) : fs::current_path();
#elif defined(__APPLE__)
        const char* home = std::getenv("HOME");
        return home ? fs::path(home) / "Library" / "Application Support" : fs::current_path();
#else
        const char* home = std::getenv("HOME");
        return home ? fs::path(home) / ".local" / "share" : fs::current_path();
#endif
    }

    static fs::path getLauncherPath() {
        return getAppDataPath() / "KonamiClient";
    }

    static fs::path getConfigPath() {
        return getLauncherPath() / "config.json";
    }

    static fs::path getCachePath() {
        return getLauncherPath() / "cache";
    }

    static fs::path getLogsPath() {
        return getLauncherPath() / "logs";
    }

    static fs::path getProfilesPath() {
        return getLauncherPath() / "profiles";
    }

    static fs::path getSkinsPath() {
        return getLauncherPath() / "skins";
    }

    static fs::path getMinecraftPath() {
#ifdef _WIN32
        const char* appData = std::getenv("APPDATA");
        return appData ? fs::path(appData) / ".minecraft" : fs::current_path();
#elif defined(__APPLE__)
        const char* home = std::getenv("HOME");
        return home ? fs::path(home) / "Library" / "Application Support" / "minecraft" : fs::current_path();
#else
        const char* home = std::getenv("HOME");
        return home ? fs::path(home) / ".minecraft" : fs::current_path();
#endif
    }
};

} // namespace konami::utils
