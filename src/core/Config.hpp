#pragma once

/**
 * Config.hpp
 * 
 * Configuration management using JSON.
 * Provides type-safe access to configuration values with defaults.
 */

#include <nlohmann/json.hpp>
#include <string>
#include <optional>
#include <mutex>
#include <fstream>
#include <filesystem>

namespace konami::core {

using json = nlohmann::json;

/**
 * Configuration manager - Thread-safe singleton
 * 
 * Manages application settings with:
 * - Type-safe getters with defaults
 * - JSON persistence
 * - Hot-reload support
 */
class Config {
public:
    /**
     * Get singleton instance
     * @return Reference to Config instance
     */
    static Config& instance() {
        static Config instance;
        return instance;
    }
    
    /**
     * Load configuration from file
     * @param path Path to config file
     * @return true if loaded successfully
     */
    bool load(const std::string& path) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        try {
            if (!std::filesystem::exists(path)) {
                return false;
            }
            
            std::ifstream file(path);
            if (!file.is_open()) {
                return false;
            }
            
            m_config = json::parse(file);
            m_configPath = path;
            return true;
            
        } catch (const json::exception& e) {
            return false;
        }
    }
    
    /**
     * Save configuration to file
     * @param path Path to config file (uses loaded path if empty)
     * @return true if saved successfully
     */
    bool save(const std::string& path = "") {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::string savePath = path.empty() ? m_configPath : path;
        if (savePath.empty()) {
            return false;
        }
        
        try {
            std::filesystem::create_directories(
                std::filesystem::path(savePath).parent_path()
            );
            
            std::ofstream file(savePath);
            if (!file.is_open()) {
                return false;
            }
            
            file << m_config.dump(4);
            return true;
            
        } catch (const std::exception& e) {
            return false;
        }
    }
    
    /**
     * Set default configuration values
     */
    void setDefaults() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        m_config = {
            {"version", "1.0.0"},
            {"theme", {
                {"current", "cyberpunk"},
                {"custom", json::object()}
            }},
            {"window", {
                {"width", 1280},
                {"height", 800},
                {"maximized", false},
                {"x", -1},
                {"y", -1}
            }},
            {"java", {
                {"autoDetect", true},
                {"paths", json::array()},
                {"defaultMemory", 4096},
                {"minMemory", 2048},
                {"maxMemory", 8192},
                {"jvmArgs", "-XX:+UseG1GC -XX:+ParallelRefProcEnabled"}
            }},
            {"downloads", {
                {"maxConcurrent", 10},
                {"retryCount", 3},
                {"retryDelay", 1000},
                {"timeout", 30000},
                {"verifyChecksums", true}
            }},
            {"game", {
                {"directory", ""},
                {"closeOnLaunch", false},
                {"showGameLog", true},
                {"fullscreen", false},
                {"resolution", {
                    {"width", 854},
                    {"height", 480}
                }}
            }},
            {"ui", {
                {"animations", true},
                {"animationSpeed", 1.0},
                {"reduceMotion", false},
                {"language", "en"},
                {"showFps", false}
            }},
            {"privacy", {
                {"analytics", false},
                {"crashReports", true}
            }},
            {"advanced", {
                {"debugMode", false},
                {"experimentalFeatures", false},
                {"cacheSize", 1024}
            }}
        };
    }
    
    /**
     * Get configuration value with dot notation
     * @param key Key path (e.g., "theme.current")
     * @param defaultValue Default value if key not found
     * @return Configuration value
     */
    template<typename T>
    T get(const std::string& key, const T& defaultValue = T{}) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        try {
            json::json_pointer ptr = toJsonPointer(key);
            if (m_config.contains(ptr)) {
                return m_config.at(ptr).get<T>();
            }
        } catch (const json::exception&) {
            // Fall through to default
        }
        
        return defaultValue;
    }
    
    /**
     * Set configuration value with dot notation
     * @param key Key path (e.g., "theme.current")
     * @param value Value to set
     */
    template<typename T>
    void set(const std::string& key, const T& value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        try {
            json::json_pointer ptr = toJsonPointer(key);
            m_config[ptr] = value;
        } catch (const json::exception& e) {
            // Log error
        }
    }
    
    /**
     * Check if key exists
     * @param key Key path
     * @return true if key exists
     */
    bool has(const std::string& key) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        try {
            json::json_pointer ptr = toJsonPointer(key);
            return m_config.contains(ptr);
        } catch (const json::exception&) {
            return false;
        }
    }
    
    /**
     * Remove configuration key
     * @param key Key path
     */
    void remove(const std::string& key) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        try {
            json::json_pointer ptr = toJsonPointer(key);
            if (!m_config.contains(ptr)) {
                return;
            }
            
            // Navigate to parent and erase the leaf key
            json::json_pointer parent = ptr.parent_pointer();
            std::string leafKey = ptr.back();
            
            if (parent.empty()) {
                m_config.erase(leafKey);
            } else if (m_config.contains(parent)) {
                m_config.at(parent).erase(leafKey);
            }
        } catch (const json::exception&) {
            // Key doesn't exist or invalid path
        }
    }
    
    /**
     * Get entire configuration as JSON
     * @return JSON configuration object
     */
    json getAll() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_config;
    }
    
    /**
     * Merge configuration values
     * @param other JSON object to merge
     */
    void merge(const json& other) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config.merge_patch(other);
    }

private:
    Config() {
        setDefaults();
    }
    
    ~Config() = default;
    
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    
    /**
     * Convert dot notation to JSON pointer
     * @param key Dot-notation key
     * @return JSON pointer
     */
    static json::json_pointer toJsonPointer(const std::string& key) {
        std::string pointer = "/";
        for (char c : key) {
            if (c == '.') {
                pointer += '/';
            } else {
                pointer += c;
            }
        }
        return json::json_pointer(pointer);
    }

private:
    mutable std::mutex m_mutex;
    json m_config;
    std::string m_configPath;
};

} // namespace konami::core
