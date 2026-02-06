// Konami Client - File Utilities
// Cross-platform file system operations

#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <optional>
#include <fstream>

namespace fs = std::filesystem;

namespace konami::utils {

/**
 * @brief File and directory utilities
 */
class FileUtils {
public:
    // Path operations
    static fs::path getAppDataPath();
    static fs::path getConfigPath();
    static fs::path getCachePath();
    static fs::path getLogsPath();
    static fs::path getProfilesPath();
    static fs::path getSkinsPath();
    static fs::path getMinecraftPath();
    static fs::path getJavaPath();
    static fs::path getTempPath();
    
    // Directory operations
    static bool createDirectory(const fs::path& path);
    static bool createDirectories(const fs::path& path);
    static bool removeDirectory(const fs::path& path);
    static bool removeDirectoryRecursive(const fs::path& path);
    static bool directoryExists(const fs::path& path);
    static std::vector<fs::path> listDirectory(const fs::path& path);
    static std::vector<fs::path> listFiles(const fs::path& path, const std::string& extension = "");
    static std::vector<fs::path> listDirectories(const fs::path& path);
    static int64_t getDirectorySize(const fs::path& path);
    
    // File operations
    static bool fileExists(const fs::path& path);
    static bool copyFile(const fs::path& source, const fs::path& destination, bool overwrite = false);
    static bool moveFile(const fs::path& source, const fs::path& destination);
    static bool deleteFile(const fs::path& path);
    static bool renameFile(const fs::path& path, const std::string& newName);
    static int64_t getFileSize(const fs::path& path);
    static std::string getFileExtension(const fs::path& path);
    static std::string getFileName(const fs::path& path);
    static std::string getFileNameWithoutExtension(const fs::path& path);
    static fs::path getParentDirectory(const fs::path& path);
    
    // Read/Write operations
    static std::optional<std::string> readFile(const fs::path& path);
    static std::optional<std::vector<uint8_t>> readBinaryFile(const fs::path& path);
    static bool writeFile(const fs::path& path, const std::string& content);
    static bool writeBinaryFile(const fs::path& path, const std::vector<uint8_t>& data);
    static bool appendFile(const fs::path& path, const std::string& content);
    static std::vector<std::string> readLines(const fs::path& path);
    
    // Hash operations
    static std::string calculateSHA1(const fs::path& path);
    static std::string calculateSHA256(const fs::path& path);
    static std::string calculateSHA512(const fs::path& path);
    static std::string calculateMD5(const fs::path& path);
    static bool verifySHA1(const fs::path& path, const std::string& expectedHash);
    static bool verifySHA256(const fs::path& path, const std::string& expectedHash);
    
    // Archive operations
    static bool extractZip(const fs::path& zipPath, const fs::path& destination);
    static bool createZip(const fs::path& sourcePath, const fs::path& zipPath);
    static std::vector<std::string> listZipContents(const fs::path& zipPath);
    static bool extractFileFromZip(const fs::path& zipPath, const std::string& fileName,
                                    const fs::path& destination);
    
    // Java JAR operations
    static std::optional<std::string> readJarManifest(const fs::path& jarPath);
    static std::vector<std::string> getJarClasses(const fs::path& jarPath);
    static bool extractFromJar(const fs::path& jarPath, const std::string& entryPath,
                               const fs::path& destination);
    
    // Temporary files
    static fs::path createTempFile(const std::string& prefix = "konami_");
    static fs::path createTempDirectory(const std::string& prefix = "konami_");
    static void cleanupTempFiles();
    
    // Path utilities
    static std::string normalizePath(const std::string& path);
    static std::string toNativePath(const std::string& path);
    static bool isAbsolutePath(const std::string& path);
    static std::string relativePath(const fs::path& path, const fs::path& base);
    static fs::path joinPath(const fs::path& base, const std::string& relative);
    
    // Platform-specific
    static std::string getExecutablePath();
    static std::string getExecutableDirectory();
    static bool isWritable(const fs::path& path);
    static bool isReadable(const fs::path& path);
    
    // File watching (basic implementation)
    static bool hasFileChanged(const fs::path& path, const std::chrono::system_clock::time_point& since);
    static std::chrono::system_clock::time_point getLastModified(const fs::path& path);
};

/**
 * @brief RAII file lock
 */
class FileLock {
public:
    FileLock(const fs::path& path);
    ~FileLock();
    
    FileLock(const FileLock&) = delete;
    FileLock& operator=(const FileLock&) = delete;
    
    bool isLocked() const { return m_locked; }
    void unlock();
    
private:
    fs::path m_path;
    bool m_locked{false};
    
#ifdef _WIN32
    void* m_handle{nullptr};
#else
    int m_fd{-1};
#endif
};

/**
 * @brief Scoped directory change
 */
class ScopedDirectory {
public:
    ScopedDirectory(const fs::path& path);
    ~ScopedDirectory();
    
    ScopedDirectory(const ScopedDirectory&) = delete;
    ScopedDirectory& operator=(const ScopedDirectory&) = delete;
    
private:
    fs::path m_previousPath;
};

} // namespace konami::utils
