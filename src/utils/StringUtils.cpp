/**
 * StringUtils.cpp
 * 
 * String manipulation and formatting utilities.
 */

#include "StringUtils.hpp"

#include <cctype>
#include <cstdarg>
#include <numeric>
#include <random>
#include <iomanip>

namespace konami::utils {

// -- Trimming --

std::string StringUtils::trim(const std::string& str) {
    auto start = str.find_first_not_of(" \t\n\r\f\v");
    if (start == std::string::npos) return "";
    auto end = str.find_last_not_of(" \t\n\r\f\v");
    return str.substr(start, end - start + 1);
}

std::string StringUtils::trimLeft(const std::string& str) {
    auto start = str.find_first_not_of(" \t\n\r\f\v");
    return start == std::string::npos ? "" : str.substr(start);
}

std::string StringUtils::trimRight(const std::string& str) {
    auto end = str.find_last_not_of(" \t\n\r\f\v");
    return end == std::string::npos ? "" : str.substr(0, end + 1);
}

// -- Case conversion --

std::string StringUtils::toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::string StringUtils::toUpper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

std::string StringUtils::capitalize(const std::string& str) {
    if (str.empty()) return str;
    std::string result = str;
    result[0] = static_cast<char>(std::toupper(result[0]));
    return result;
}

std::string StringUtils::titleCase(const std::string& str) {
    std::string result = str;
    bool capitalize_next = true;
    for (char& c : result) {
        if (std::isspace(c)) { capitalize_next = true; }
        else if (capitalize_next) { c = static_cast<char>(std::toupper(c)); capitalize_next = false; }
    }
    return result;
}

// -- Split/Join --

std::vector<std::string> StringUtils::split(const std::string& str, char delimiter) {
    std::vector<std::string> parts;
    std::istringstream iss(str);
    std::string part;
    while (std::getline(iss, part, delimiter)) parts.push_back(part);
    return parts;
}

std::vector<std::string> StringUtils::split(const std::string& str, const std::string& delimiter) {
    std::vector<std::string> parts;
    size_t pos = 0, prev = 0;
    while ((pos = str.find(delimiter, prev)) != std::string::npos) {
        parts.push_back(str.substr(prev, pos - prev));
        prev = pos + delimiter.size();
    }
    parts.push_back(str.substr(prev));
    return parts;
}

std::string StringUtils::join(const std::vector<std::string>& parts, const std::string& separator) {
    if (parts.empty()) return "";
    std::string result = parts[0];
    for (size_t i = 1; i < parts.size(); ++i) result += separator + parts[i];
    return result;
}

// -- Search/Replace --

std::string StringUtils::replace(const std::string& str, const std::string& from, const std::string& to) {
    std::string result = str;
    auto pos = result.find(from);
    if (pos != std::string::npos) result.replace(pos, from.size(), to);
    return result;
}

std::string StringUtils::replaceAll(const std::string& str, const std::string& from, const std::string& to) {
    std::string result = str;
    size_t pos = 0;
    while ((pos = result.find(from, pos)) != std::string::npos) {
        result.replace(pos, from.size(), to);
        pos += to.size();
    }
    return result;
}

bool StringUtils::contains(const std::string& str, const std::string& substr) {
    return str.find(substr) != std::string::npos;
}

bool StringUtils::startsWith(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}

bool StringUtils::endsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// -- Padding --

std::string StringUtils::padLeft(const std::string& str, size_t length, char padChar) {
    if (str.size() >= length) return str;
    return std::string(length - str.size(), padChar) + str;
}

std::string StringUtils::padRight(const std::string& str, size_t length, char padChar) {
    if (str.size() >= length) return str;
    return str + std::string(length - str.size(), padChar);
}

std::string StringUtils::center(const std::string& str, size_t length, char padChar) {
    if (str.size() >= length) return str;
    size_t leftPad = (length - str.size()) / 2;
    size_t rightPad = length - str.size() - leftPad;
    return std::string(leftPad, padChar) + str + std::string(rightPad, padChar);
}

// -- Formatting --

std::string StringUtils::formatBytes(int64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double size = static_cast<double>(bytes);
    int unit = 0;
    while (size >= 1024.0 && unit < 4) { size /= 1024.0; ++unit; }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(unit == 0 ? 0 : 1) << size << " " << units[unit];
    return oss.str();
}

std::string StringUtils::formatDuration(std::chrono::seconds duration) {
    auto h = std::chrono::duration_cast<std::chrono::hours>(duration);
    auto m = std::chrono::duration_cast<std::chrono::minutes>(duration - h);
    auto s = duration - h - m;
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << h.count() << ":"
        << std::setfill('0') << std::setw(2) << m.count() << ":"
        << std::setfill('0') << std::setw(2) << s.count();
    return oss.str();
}

std::string StringUtils::formatTimestamp(std::chrono::system_clock::time_point time, const std::string& format) {
    auto tt = std::chrono::system_clock::to_time_t(time);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, format.c_str());
    return oss.str();
}

std::string StringUtils::formatNumber(int64_t number) {
    std::string str = std::to_string(number);
    int n = static_cast<int>(str.size());
    for (int i = n - 3; i > 0; i -= 3) str.insert(i, ",");
    return str;
}

std::string StringUtils::formatPercentage(double value, int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << (value * 100.0) << "%";
    return oss.str();
}

// -- Encoding --

static const std::string BASE64_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string StringUtils::base64Encode(const std::string& str) {
    return base64Encode(std::vector<uint8_t>(str.begin(), str.end()));
}

std::string StringUtils::base64Encode(const std::vector<uint8_t>& data) {
    std::string result;
    int val = 0, valb = -6;
    for (uint8_t c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            result.push_back(BASE64_CHARS[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) result.push_back(BASE64_CHARS[((val << 8) >> (valb + 8)) & 0x3F]);
    while (result.size() % 4) result.push_back('=');
    return result;
}

std::vector<uint8_t> StringUtils::base64Decode(const std::string& encoded) {
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[static_cast<unsigned char>(BASE64_CHARS[i])] = i;
    std::vector<uint8_t> out;
    int val = 0, valb = -8;
    for (unsigned char c : encoded) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

std::string StringUtils::hexEncode(const std::vector<uint8_t>& data) {
    std::ostringstream oss;
    for (uint8_t b : data) oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(b);
    return oss.str();
}

std::vector<uint8_t> StringUtils::hexDecode(const std::string& hex) {
    std::vector<uint8_t> result;
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        result.push_back(static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    }
    return result;
}

// -- UUID --

std::string StringUtils::generateUUID() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char hex[] = "0123456789abcdef";

    std::string uuid(36, '-');
    for (int i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) continue;
        uuid[i] = hex[dis(gen)];
    }
    uuid[14] = '4'; // version 4
    uuid[19] = hex[(dis(gen) & 0x3) | 0x8]; // variant
    return uuid;
}

bool StringUtils::isValidUUID(const std::string& str) {
    if (str.size() != 36) return false;
    for (int i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (str[i] != '-') return false;
        } else {
            if (!std::isxdigit(str[i])) return false;
        }
    }
    return true;
}

std::string StringUtils::formatUUID(const std::string& uuid) {
    std::string stripped = replaceAll(uuid, "-", "");
    if (stripped.size() != 32) return uuid;
    return stripped.substr(0,8) + "-" + stripped.substr(8,4) + "-" +
           stripped.substr(12,4) + "-" + stripped.substr(16,4) + "-" + stripped.substr(20,12);
}

std::string StringUtils::stripUUID(const std::string& uuid) {
    return replaceAll(uuid, "-", "");
}

// -- Validation --

bool StringUtils::isAlpha(const std::string& str) { return !str.empty() && std::all_of(str.begin(), str.end(), ::isalpha); }
bool StringUtils::isNumeric(const std::string& str) { return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit); }
bool StringUtils::isAlphanumeric(const std::string& str) { return !str.empty() && std::all_of(str.begin(), str.end(), ::isalnum); }
bool StringUtils::isEmail(const std::string& str) { return str.find('@') != std::string::npos && str.find('.') != std::string::npos; }
bool StringUtils::isUrl(const std::string& str) { return startsWith(str, "http://") || startsWith(str, "https://"); }
bool StringUtils::isEmpty(const std::string& str) { return str.empty(); }
bool StringUtils::isBlank(const std::string& str) { return trim(str).empty(); }

// -- Minecraft specific --

bool StringUtils::isValidMinecraftUsername(const std::string& username) {
    if (username.size() < 3 || username.size() > 16) return false;
    return std::all_of(username.begin(), username.end(), [](char c) {
        return std::isalnum(c) || c == '_';
    });
}

bool StringUtils::isValidVersion(const std::string& version) {
    return !version.empty() && version.find('.') != std::string::npos;
}

std::string StringUtils::sanitizeFileName(const std::string& name) {
    std::string result;
    for (char c : name) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.') result += c;
        else result += '_';
    }
    return result;
}

// -- Parsing --

int StringUtils::parseInt(const std::string& str, int defaultValue) {
    try { return std::stoi(str); } catch (...) { return defaultValue; }
}

int64_t StringUtils::parseLong(const std::string& str, int64_t defaultValue) {
    try { return std::stoll(str); } catch (...) { return defaultValue; }
}

double StringUtils::parseDouble(const std::string& str, double defaultValue) {
    try { return std::stod(str); } catch (...) { return defaultValue; }
}

bool StringUtils::parseBool(const std::string& str, bool defaultValue) {
    auto lower = toLower(trim(str));
    if (lower == "true" || lower == "1" || lower == "yes") return true;
    if (lower == "false" || lower == "0" || lower == "no") return false;
    return defaultValue;
}

// -- Version comparison --

int StringUtils::compareVersions(const std::string& v1, const std::string& v2) {
    auto parts1 = split(v1, '.');
    auto parts2 = split(v2, '.');
    size_t max = std::max(parts1.size(), parts2.size());
    for (size_t i = 0; i < max; ++i) {
        int p1 = i < parts1.size() ? parseInt(parts1[i]) : 0;
        int p2 = i < parts2.size() ? parseInt(parts2[i]) : 0;
        if (p1 < p2) return -1;
        if (p1 > p2) return 1;
    }
    return 0;
}

bool StringUtils::isVersionNewer(const std::string& version, const std::string& than) {
    return compareVersions(version, than) > 0;
}

// -- Escaping --

std::string StringUtils::escapeJson(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\t': result += "\\t"; break;
            case '\r': result += "\\r"; break;
            default: result += c;
        }
    }
    return result;
}

std::string StringUtils::escapeHtml(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '"': result += "&quot;"; break;
            case '\'': result += "&#39;"; break;
            default: result += c;
        }
    }
    return result;
}

std::string StringUtils::escapeShell(const std::string& str) {
    std::string result = "'";
    for (char c : str) {
        if (c == '\'') result += "'\\''";
        else result += c;
    }
    result += "'";
    return result;
}

std::string StringUtils::escapeRegex(const std::string& str) {
    static const std::string metaChars = R"(\.^$|()[]{}*+?)";
    std::string result;
    for (char c : str) {
        if (metaChars.find(c) != std::string::npos) result += '\\';
        result += c;
    }
    return result;
}

// -- Truncation --

std::string StringUtils::truncate(const std::string& str, size_t maxLength, const std::string& suffix) {
    if (str.size() <= maxLength) return str;
    return str.substr(0, maxLength - suffix.size()) + suffix;
}

std::string StringUtils::ellipsis(const std::string& str, size_t maxLength) {
    return truncate(str, maxLength, "...");
}

// -- Word wrapping --

std::vector<std::string> StringUtils::wordWrap(const std::string& str, size_t lineWidth) {
    std::vector<std::string> lines;
    std::istringstream iss(str);
    std::string word, line;
    while (iss >> word) {
        if (line.size() + word.size() + 1 > lineWidth) {
            if (!line.empty()) lines.push_back(line);
            line = word;
        } else {
            if (!line.empty()) line += ' ';
            line += word;
        }
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

// -- StringBuilder --

StringBuilder& StringBuilder::append(const std::string& str) { m_stream << str; return *this; }
StringBuilder& StringBuilder::append(char c) { m_stream << c; return *this; }
StringBuilder& StringBuilder::append(int value) { m_stream << value; return *this; }
StringBuilder& StringBuilder::append(int64_t value) { m_stream << value; return *this; }
StringBuilder& StringBuilder::append(double value) { m_stream << value; return *this; }
StringBuilder& StringBuilder::appendLine(const std::string& str) { m_stream << str << '\n'; return *this; }
StringBuilder& StringBuilder::appendFormat(const std::string& /*fmt*/, ...) { return *this; /* TODO */ }

std::string StringBuilder::toString() const { return m_stream.str(); }
void StringBuilder::clear() { m_stream.str(""); m_stream.clear(); }
size_t StringBuilder::length() const { return m_stream.str().size(); }
bool StringBuilder::isEmpty() const { return m_stream.str().empty(); }

// -- Levenshtein distance --

int levenshteinDistance(const std::string& s1, const std::string& s2) {
    size_t m = s1.size(), n = s2.size();
    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1));
    for (size_t i = 0; i <= m; ++i) dp[i][0] = static_cast<int>(i);
    for (size_t j = 0; j <= n; ++j) dp[0][j] = static_cast<int>(j);
    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
            int cost = (s1[i-1] == s2[j-1]) ? 0 : 1;
            dp[i][j] = std::min({dp[i-1][j]+1, dp[i][j-1]+1, dp[i-1][j-1]+cost});
        }
    }
    return dp[m][n];
}

std::string findBestMatch(const std::string& query, const std::vector<std::string>& candidates, int maxDistance) {
    std::string best;
    int bestDist = maxDistance + 1;
    for (const auto& c : candidates) {
        int dist = levenshteinDistance(query, c);
        if (dist < bestDist) { bestDist = dist; best = c; }
    }
    return best;
}

} // namespace konami::utils
