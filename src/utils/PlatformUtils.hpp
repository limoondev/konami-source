// Konami Client - Platform Utilities
// Cross-platform system utilities

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <filesystem>

namespace konami::utils {

/**
 * @brief Operating system type
 */
enum class OS {
    Windows,
    macOS,
    Linux,
    Unknown
};

/**
 * @brief CPU architecture
 */
enum class Architecture {
    x86,
    x64,
    ARM,
    ARM64,
    Unknown
};

/**
 * @brief System information
 */
struct SystemInfo {
    OS os;
    Architecture arch;
    std::string osName;
    std::string osVersion;
    std::string kernelVersion;
    std::string hostname;
    std::string username;
    int cpuCores;
    int64_t totalMemory;
    int64_t availableMemory;
    std::string cpuModel;
    std::string gpuModel;
    std::string gpuVendor;
    bool hasVulkan;
    bool hasOpenGL;
    bool hasMetal;
    bool hasDirectX12;
};

/**
 * @brief Java installation info
 */
struct JavaInfo {
    std::filesystem::path path;
    std::string version;
    std::string vendor;
    int majorVersion;
    bool is64Bit;
    bool isJDK;
    bool isValid;
};

/**
 * @brief GPU information
 */
struct GPUInfo {
    std::string name;
    std::string vendor;
    std::string driver;
    int64_t vramBytes;
    bool supportsVulkan;
    bool supportsOpenGL;
    bool supportsMetal;
    bool supportsDirectX12;
    std::string vulkanVersion;
    std::string openGLVersion;
};

/**
 * @brief Platform-specific utilities
 */
class PlatformUtils {
public:
    // OS detection
    static OS getOS();
    static std::string getOSName();
    static std::string getOSVersion();
    static Architecture getArchitecture();
    static std::string getArchitectureName();
    static bool is64Bit();
    
    // System info
    static SystemInfo getSystemInfo();
    static int64_t getTotalMemory();
    static int64_t getAvailableMemory();
    static int getCPUCores();
    static std::string getCPUModel();
    static double getCPUUsage();
    static std::string getUsername();
    static std::string getHostname();
    
    // GPU info
    static std::vector<GPUInfo> getGPUs();
    static GPUInfo getPrimaryGPU();
    static bool hasVulkanSupport();
    static bool hasOpenGLSupport();
    static bool hasMetalSupport();
    static bool hasDirectX12Support();
    
    // Java detection
    static std::vector<JavaInfo> findJavaInstallations();
    static std::optional<JavaInfo> findJava(int minVersion = 17);
    static std::optional<JavaInfo> getJavaInfo(const std::filesystem::path& javaPath);
    static std::string getRecommendedJavaArgs(int64_t maxMemoryMB);
    static bool isJavaValid(const std::filesystem::path& path);
    
    // Process management
    static int executeCommand(const std::string& command);
    static std::string executeCommandWithOutput(const std::string& command);
    static bool startProcess(const std::string& path, const std::vector<std::string>& args);
    static int startProcessAndWait(const std::string& path, const std::vector<std::string>& args);
    static bool isProcessRunning(int pid);
    static bool killProcess(int pid);
    static int getCurrentProcessId();
    
    // File associations
    static bool openUrl(const std::string& url);
    static bool openFile(const std::filesystem::path& path);
    static bool openFolder(const std::filesystem::path& path);
    static bool showInExplorer(const std::filesystem::path& path);
    
    // Environment
    static std::optional<std::string> getEnv(const std::string& name);
    static bool setEnv(const std::string& name, const std::string& value);
    static std::string expandEnvVars(const std::string& str);
    
    // Paths
    static std::filesystem::path getHomeDirectory();
    static std::filesystem::path getAppDataDirectory();
    static std::filesystem::path getTempDirectory();
    static std::filesystem::path getDocumentsDirectory();
    static std::filesystem::path getDownloadsDirectory();
    static std::filesystem::path getDesktopDirectory();
    
    // Minecraft paths
    static std::filesystem::path getDefaultMinecraftDirectory();
    static std::filesystem::path getMinecraftLauncherPath();
    
    // Native library suffix
    static std::string getNativeLibrarySuffix();
    static std::string getNativeExecutableSuffix();
    static std::string getClasspathSeparator();
    
    // Display
    static std::pair<int, int> getPrimaryDisplayResolution();
    static std::vector<std::pair<int, int>> getAvailableResolutions();
    static double getDisplayScaleFactor();
    static bool isDarkModeEnabled();
    
    // Power
    static bool isOnBattery();
    static int getBatteryPercentage();
    static bool isLaptop();
    
    // Notifications
    static bool showNotification(const std::string& title, const std::string& message,
                                  const std::string& iconPath = "");
    
    // Clipboard
    static std::string getClipboardText();
    static bool setClipboardText(const std::string& text);
    
    // Single instance
    static bool acquireSingleInstanceLock(const std::string& appName);
    static void releaseSingleInstanceLock();
    static bool isAnotherInstanceRunning(const std::string& appName);
};

/**
 * @brief Memory-mapped file for efficient large file access
 */
class MemoryMappedFile {
public:
    MemoryMappedFile();
    ~MemoryMappedFile();
    
    bool open(const std::filesystem::path& path, bool readOnly = true);
    void close();
    
    bool isOpen() const { return m_data != nullptr; }
    const uint8_t* data() const { return m_data; }
    size_t size() const { return m_size; }
    
private:
    uint8_t* m_data{nullptr};
    size_t m_size{0};
    
#ifdef _WIN32
    void* m_fileHandle{nullptr};
    void* m_mappingHandle{nullptr};
#else
    int m_fd{-1};
#endif
};

} // namespace konami::utils
