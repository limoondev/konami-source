/**
 * FileUtils.cpp
 * 
 * Cross-platform file system operations.
 */

#include "FileUtils.hpp"
#include "PathUtils.hpp"
#include "HashUtils.hpp"

#include <fstream>
#include <sstream>
#include <random>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <sys/file.h>
#endif

namespace konami::utils {

// -- Path operations --

fs::path FileUtils::getAppDataPath() { return PathUtils::getAppDataPath(); }
fs::path FileUtils::getConfigPath() { return PathUtils::getConfigPath(); }
fs::path FileUtils::getCachePath() { return PathUtils::getCachePath(); }
fs::path FileUtils::getLogsPath() { return PathUtils::getLogsPath(); }
fs::path FileUtils::getProfilesPath() { return PathUtils::getProfilesPath(); }
fs::path FileUtils::getSkinsPath() { return PathUtils::getSkinsPath(); }
fs::path FileUtils::getMinecraftPath() { return PathUtils::getMinecraftPath(); }
fs::path FileUtils::getJavaPath() { return fs::path(); } // Auto-detect
fs::path FileUtils::getTempPath() { return fs::temp_directory_path(); }

// -- Directory operations --

bool FileUtils::createDirectory(const fs::path& path) { std::error_code ec; return fs::create_directory(path, ec); }
bool FileUtils::createDirectories(const fs::path& path) { std::error_code ec; return fs::create_directories(path, ec); }
bool FileUtils::removeDirectory(const fs::path& path) { std::error_code ec; return fs::remove(path, ec); }
bool FileUtils::removeDirectoryRecursive(const fs::path& path) { std::error_code ec; return fs::remove_all(path, ec) > 0; }
bool FileUtils::directoryExists(const fs::path& path) { return fs::is_directory(path); }

std::vector<fs::path> FileUtils::listDirectory(const fs::path& path) {
    std::vector<fs::path> entries;
    if (!fs::exists(path)) return entries;
    for (const auto& e : fs::directory_iterator(path)) entries.push_back(e.path());
    return entries;
}

std::vector<fs::path> FileUtils::listFiles(const fs::path& path, const std::string& extension) {
    std::vector<fs::path> files;
    if (!fs::exists(path)) return files;
    for (const auto& e : fs::directory_iterator(path)) {
        if (e.is_regular_file()) {
            if (extension.empty() || e.path().extension() == extension) {
                files.push_back(e.path());
            }
        }
    }
    return files;
}

std::vector<fs::path> FileUtils::listDirectories(const fs::path& path) {
    std::vector<fs::path> dirs;
    if (!fs::exists(path)) return dirs;
    for (const auto& e : fs::directory_iterator(path)) {
        if (e.is_directory()) dirs.push_back(e.path());
    }
    return dirs;
}

int64_t FileUtils::getDirectorySize(const fs::path& path) {
    int64_t size = 0;
    if (!fs::exists(path)) return 0;
    for (const auto& e : fs::recursive_directory_iterator(path)) {
        if (e.is_regular_file()) size += static_cast<int64_t>(e.file_size());
    }
    return size;
}

// -- File operations --

bool FileUtils::fileExists(const fs::path& path) { return fs::is_regular_file(path); }

bool FileUtils::copyFile(const fs::path& source, const fs::path& destination, bool overwrite) {
    std::error_code ec;
    auto opts = overwrite ? fs::copy_options::overwrite_existing : fs::copy_options::none;
    return fs::copy_file(source, destination, opts, ec);
}

bool FileUtils::moveFile(const fs::path& source, const fs::path& destination) {
    std::error_code ec; fs::rename(source, destination, ec); return !ec;
}

bool FileUtils::deleteFile(const fs::path& path) { std::error_code ec; return fs::remove(path, ec); }

bool FileUtils::renameFile(const fs::path& path, const std::string& newName) {
    auto newPath = path.parent_path() / newName;
    std::error_code ec; fs::rename(path, newPath, ec); return !ec;
}

int64_t FileUtils::getFileSize(const fs::path& path) {
    std::error_code ec; return static_cast<int64_t>(fs::file_size(path, ec));
}

std::string FileUtils::getFileExtension(const fs::path& path) { return path.extension().string(); }
std::string FileUtils::getFileName(const fs::path& path) { return path.filename().string(); }
std::string FileUtils::getFileNameWithoutExtension(const fs::path& path) { return path.stem().string(); }
fs::path FileUtils::getParentDirectory(const fs::path& path) { return path.parent_path(); }

// -- Read/Write --

std::optional<std::string> FileUtils::readFile(const fs::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) return std::nullopt;
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

std::optional<std::vector<uint8_t>> FileUtils::readBinaryFile(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return std::nullopt;
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

bool FileUtils::writeFile(const fs::path& path, const std::string& content) {
    createDirectories(path.parent_path());
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << content;
    return true;
}

bool FileUtils::writeBinaryFile(const fs::path& path, const std::vector<uint8_t>& data) {
    createDirectories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return true;
}

bool FileUtils::appendFile(const fs::path& path, const std::string& content) {
    std::ofstream file(path, std::ios::app);
    if (!file.is_open()) return false;
    file << content;
    return true;
}

std::vector<std::string> FileUtils::readLines(const fs::path& path) {
    std::vector<std::string> lines;
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) lines.push_back(line);
    return lines;
}

// -- Hash operations --

std::string FileUtils::calculateSHA1(const fs::path& path) { return HashUtils::sha1File(path.string()); }
std::string FileUtils::calculateSHA256(const fs::path& path) { return HashUtils::sha256File(path.string()); }
std::string FileUtils::calculateSHA512(const fs::path& /*path*/) { return ""; /* TODO */ }
std::string FileUtils::calculateMD5(const fs::path& /*path*/) { return ""; /* TODO */ }
bool FileUtils::verifySHA1(const fs::path& path, const std::string& expectedHash) { return calculateSHA1(path) == expectedHash; }
bool FileUtils::verifySHA256(const fs::path& path, const std::string& expectedHash) { return calculateSHA256(path) == expectedHash; }

// -- Archive operations (stubs) --

bool FileUtils::extractZip(const fs::path& /*zipPath*/, const fs::path& /*destination*/) { return false; /* TODO: Use minizip */ }
bool FileUtils::createZip(const fs::path& /*sourcePath*/, const fs::path& /*zipPath*/) { return false; }
std::vector<std::string> FileUtils::listZipContents(const fs::path& /*zipPath*/) { return {}; }
bool FileUtils::extractFileFromZip(const fs::path& /*zipPath*/, const std::string& /*fileName*/, const fs::path& /*destination*/) { return false; }

// -- JAR operations (stubs) --

std::optional<std::string> FileUtils::readJarManifest(const fs::path& /*jarPath*/) { return std::nullopt; }
std::vector<std::string> FileUtils::getJarClasses(const fs::path& /*jarPath*/) { return {}; }
bool FileUtils::extractFromJar(const fs::path& /*jarPath*/, const std::string& /*entryPath*/, const fs::path& /*destination*/) { return false; }

// -- Temp files --

fs::path FileUtils::createTempFile(const std::string& prefix) {
    auto temp = fs::temp_directory_path() / (prefix + std::to_string(std::random_device{}()));
    std::ofstream(temp).close();
    return temp;
}

fs::path FileUtils::createTempDirectory(const std::string& prefix) {
    auto temp = fs::temp_directory_path() / (prefix + std::to_string(std::random_device{}()));
    fs::create_directories(temp);
    return temp;
}

void FileUtils::cleanupTempFiles() { /* TODO */ }

// -- Path utilities --

std::string FileUtils::normalizePath(const std::string& path) { return fs::path(path).lexically_normal().string(); }
std::string FileUtils::toNativePath(const std::string& path) { return fs::path(path).make_preferred().string(); }
bool FileUtils::isAbsolutePath(const std::string& path) { return fs::path(path).is_absolute(); }
std::string FileUtils::relativePath(const fs::path& path, const fs::path& base) { return fs::relative(path, base).string(); }
fs::path FileUtils::joinPath(const fs::path& base, const std::string& relative) { return base / relative; }

// -- Platform-specific --

std::string FileUtils::getExecutablePath() {
#ifdef _WIN32
    char buf[MAX_PATH]; GetModuleFileNameA(nullptr, buf, MAX_PATH); return buf;
#elif defined(__APPLE__)
    return ""; /* TODO: use _NSGetExecutablePath */
#else
    char buf[4096]; auto len = readlink("/proc/self/exe", buf, sizeof(buf)-1);
    return len > 0 ? std::string(buf, len) : "";
#endif
}

std::string FileUtils::getExecutableDirectory() {
    return fs::path(getExecutablePath()).parent_path().string();
}

bool FileUtils::isWritable(const fs::path& path) {
    auto status = fs::status(path);
    auto perms = status.permissions();
    return (perms & fs::perms::owner_write) != fs::perms::none;
}

bool FileUtils::isReadable(const fs::path& path) {
    auto status = fs::status(path);
    auto perms = status.permissions();
    return (perms & fs::perms::owner_read) != fs::perms::none;
}

bool FileUtils::hasFileChanged(const fs::path& path, const std::chrono::system_clock::time_point& since) {
    return getLastModified(path) > since;
}

std::chrono::system_clock::time_point FileUtils::getLastModified(const fs::path& path) {
    // Simplified: return current time if the file doesn't exist
    if (!fs::exists(path)) return std::chrono::system_clock::now();
    auto ftime = fs::last_write_time(path);
    // C++20 conversion
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    return sctp;
}

// -- FileLock --

FileLock::FileLock(const fs::path& path) : m_path(path) {
#ifdef _WIN32
    m_handle = CreateFileA(path.string().c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    m_locked = (m_handle != INVALID_HANDLE_VALUE);
#else
    m_fd = open(path.string().c_str(), O_CREAT | O_WRONLY, 0644);
    if (m_fd >= 0) {
        m_locked = flock(m_fd, LOCK_EX | LOCK_NB) == 0;
    }
#endif
}

FileLock::~FileLock() { unlock(); }

void FileLock::unlock() {
    if (!m_locked) return;
#ifdef _WIN32
    if (m_handle) { CloseHandle(m_handle); m_handle = nullptr; }
#else
    if (m_fd >= 0) { flock(m_fd, LOCK_UN); close(m_fd); m_fd = -1; }
#endif
    m_locked = false;
    std::error_code ec;
    fs::remove(m_path, ec);
}

// -- ScopedDirectory --

ScopedDirectory::ScopedDirectory(const fs::path& path) : m_previousPath(fs::current_path()) {
    fs::current_path(path);
}

ScopedDirectory::~ScopedDirectory() {
    std::error_code ec;
    fs::current_path(m_previousPath, ec);
}

} // namespace konami::utils
