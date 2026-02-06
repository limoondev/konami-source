// Konami Client - JSON Utilities
// JSON parsing and manipulation helpers

#pragma once

#include <string>
#include <optional>
#include <vector>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace konami::utils {

using json = nlohmann::json;

/**
 * @brief JSON utility functions
 */
class JsonUtils {
public:
    // Parsing
    static std::optional<json> parse(const std::string& str);
    static std::optional<json> parseFile(const std::filesystem::path& path);
    static bool isValid(const std::string& str);
    
    // Serialization
    static std::string stringify(const json& j, int indent = -1);
    static std::string prettyPrint(const json& j, int indent = 2);
    static bool writeFile(const std::filesystem::path& path, const json& j, int indent = 2);
    
    // Safe accessors
    static std::string getString(const json& j, const std::string& key, const std::string& defaultValue = "");
    static int getInt(const json& j, const std::string& key, int defaultValue = 0);
    static int64_t getLong(const json& j, const std::string& key, int64_t defaultValue = 0);
    static double getDouble(const json& j, const std::string& key, double defaultValue = 0.0);
    static bool getBool(const json& j, const std::string& key, bool defaultValue = false);
    static json getObject(const json& j, const std::string& key, const json& defaultValue = json::object());
    static json getArray(const json& j, const std::string& key, const json& defaultValue = json::array());
    
    // Nested access with dot notation
    static std::optional<json> getPath(const json& j, const std::string& path);
    static std::string getPathString(const json& j, const std::string& path, const std::string& defaultValue = "");
    static int getPathInt(const json& j, const std::string& path, int defaultValue = 0);
    static bool getPathBool(const json& j, const std::string& path, bool defaultValue = false);
    
    // Set nested value
    static void setPath(json& j, const std::string& path, const json& value);
    
    // Type checking
    static bool hasKey(const json& j, const std::string& key);
    static bool isString(const json& j, const std::string& key);
    static bool isNumber(const json& j, const std::string& key);
    static bool isObject(const json& j, const std::string& key);
    static bool isArray(const json& j, const std::string& key);
    static bool isBool(const json& j, const std::string& key);
    static bool isNull(const json& j, const std::string& key);
    
    // Merging
    static json merge(const json& base, const json& override);
    static json deepMerge(const json& base, const json& override);
    
    // Filtering
    static json filterKeys(const json& j, const std::vector<std::string>& keys);
    static json excludeKeys(const json& j, const std::vector<std::string>& keys);
    
    // Array operations
    static json findInArray(const json& arr, const std::string& key, const std::string& value);
    static int findIndexInArray(const json& arr, const std::string& key, const std::string& value);
    static json mapArray(const json& arr, std::function<json(const json&)> mapper);
    static json filterArray(const json& arr, std::function<bool(const json&)> predicate);
    
    // Comparison
    static bool equals(const json& j1, const json& j2);
    static json diff(const json& j1, const json& j2);
    
    // Schema validation (basic)
    static bool validateSchema(const json& j, const json& schema);
    static std::vector<std::string> getValidationErrors(const json& j, const json& schema);
};

/**
 * @brief JSON Pointer wrapper for easy path access
 */
class JsonPointer {
public:
    JsonPointer(const std::string& path);
    
    std::optional<json> get(const json& j) const;
    void set(json& j, const json& value) const;
    bool exists(const json& j) const;
    void remove(json& j) const;
    
private:
    nlohmann::json::json_pointer m_pointer;
};

/**
 * @brief JSON builder for fluent construction
 */
class JsonBuilder {
public:
    JsonBuilder();
    
    JsonBuilder& set(const std::string& key, const std::string& value);
    JsonBuilder& set(const std::string& key, int value);
    JsonBuilder& set(const std::string& key, int64_t value);
    JsonBuilder& set(const std::string& key, double value);
    JsonBuilder& set(const std::string& key, bool value);
    JsonBuilder& set(const std::string& key, const json& value);
    JsonBuilder& setNull(const std::string& key);
    JsonBuilder& setArray(const std::string& key);
    JsonBuilder& setObject(const std::string& key);
    
    JsonBuilder& beginObject(const std::string& key);
    JsonBuilder& endObject();
    JsonBuilder& beginArray(const std::string& key);
    JsonBuilder& endArray();
    JsonBuilder& addToArray(const json& value);
    
    json build() const;
    std::string toString(int indent = -1) const;
    
private:
    json m_root;
    std::vector<json*> m_stack;
    std::vector<std::string> m_arrayKeys;
};

} // namespace konami::utils
