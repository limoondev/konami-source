// Konami Client - String Utilities
// String manipulation and formatting

#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <regex>
#include <iomanip>
#include <chrono>

namespace konami::utils {

/**
 * @brief String manipulation utilities
 */
class StringUtils {
public:
    // Trimming
    static std::string trim(const std::string& str);
    static std::string trimLeft(const std::string& str);
    static std::string trimRight(const std::string& str);
    
    // Case conversion
    static std::string toLower(const std::string& str);
    static std::string toUpper(const std::string& str);
    static std::string capitalize(const std::string& str);
    static std::string titleCase(const std::string& str);
    
    // Splitting and joining
    static std::vector<std::string> split(const std::string& str, char delimiter);
    static std::vector<std::string> split(const std::string& str, const std::string& delimiter);
    static std::string join(const std::vector<std::string>& parts, const std::string& separator);
    
    // Search and replace
    static std::string replace(const std::string& str, const std::string& from, const std::string& to);
    static std::string replaceAll(const std::string& str, const std::string& from, const std::string& to);
    static bool contains(const std::string& str, const std::string& substr);
    static bool startsWith(const std::string& str, const std::string& prefix);
    static bool endsWith(const std::string& str, const std::string& suffix);
    
    // Padding
    static std::string padLeft(const std::string& str, size_t length, char padChar = ' ');
    static std::string padRight(const std::string& str, size_t length, char padChar = ' ');
    static std::string center(const std::string& str, size_t length, char padChar = ' ');
    
    // Formatting
    template<typename... Args>
    static std::string format(const std::string& fmt, Args... args);
    
    static std::string formatBytes(int64_t bytes);
    static std::string formatDuration(std::chrono::seconds duration);
    static std::string formatTimestamp(std::chrono::system_clock::time_point time,
                                        const std::string& format = "%Y-%m-%d %H:%M:%S");
    static std::string formatNumber(int64_t number);
    static std::string formatPercentage(double value, int precision = 1);
    
    // Encoding
    static std::string base64Encode(const std::string& str);
    static std::string base64Encode(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> base64Decode(const std::string& encoded);
    static std::string hexEncode(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> hexDecode(const std::string& hex);
    
    // UUID
    static std::string generateUUID();
    static bool isValidUUID(const std::string& str);
    static std::string formatUUID(const std::string& uuid); // Add dashes
    static std::string stripUUID(const std::string& uuid);  // Remove dashes
    
    // Validation
    static bool isAlpha(const std::string& str);
    static bool isNumeric(const std::string& str);
    static bool isAlphanumeric(const std::string& str);
    static bool isEmail(const std::string& str);
    static bool isUrl(const std::string& str);
    static bool isEmpty(const std::string& str);
    static bool isBlank(const std::string& str);
    
    // Minecraft specific
    static bool isValidMinecraftUsername(const std::string& username);
    static bool isValidVersion(const std::string& version);
    static std::string sanitizeFileName(const std::string& name);
    
    // Parsing
    static int parseInt(const std::string& str, int defaultValue = 0);
    static int64_t parseLong(const std::string& str, int64_t defaultValue = 0);
    static double parseDouble(const std::string& str, double defaultValue = 0.0);
    static bool parseBool(const std::string& str, bool defaultValue = false);
    
    // Version comparison
    static int compareVersions(const std::string& v1, const std::string& v2);
    static bool isVersionNewer(const std::string& version, const std::string& than);
    
    // Escaping
    static std::string escapeJson(const std::string& str);
    static std::string escapeHtml(const std::string& str);
    static std::string escapeShell(const std::string& str);
    static std::string escapeRegex(const std::string& str);
    
    // Truncation
    static std::string truncate(const std::string& str, size_t maxLength, const std::string& suffix = "...");
    static std::string ellipsis(const std::string& str, size_t maxLength);
    
    // Word wrapping
    static std::vector<std::string> wordWrap(const std::string& str, size_t lineWidth);
};

// Template implementation
template<typename... Args>
std::string StringUtils::format(const std::string& fmt, Args... args) {
    // Simple format implementation using stringstream
    std::ostringstream oss;
    int dummy[] = { 0, ((void)(oss << args), 0)... };
    (void)dummy;
    return oss.str();
}

/**
 * @brief String builder for efficient concatenation
 */
class StringBuilder {
public:
    StringBuilder& append(const std::string& str);
    StringBuilder& append(char c);
    StringBuilder& append(int value);
    StringBuilder& append(int64_t value);
    StringBuilder& append(double value);
    StringBuilder& appendLine(const std::string& str = "");
    StringBuilder& appendFormat(const std::string& fmt, ...);
    
    std::string toString() const;
    void clear();
    size_t length() const;
    bool isEmpty() const;
    
private:
    std::ostringstream m_stream;
};

/**
 * @brief Levenshtein distance for fuzzy matching
 */
int levenshteinDistance(const std::string& s1, const std::string& s2);

/**
 * @brief Find best match from list
 */
std::string findBestMatch(const std::string& query, const std::vector<std::string>& candidates,
                          int maxDistance = 3);

} // namespace konami::utils
