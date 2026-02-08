/**
 * PlatformUtils.cpp
 * 
 * Cross-platform system utilities.
 */

#include "PlatformUtils.hpp"
#include "PathUtils.hpp"

#include <fstream>
#include <sstream>
#include <thread>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#else
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/sysinfo.h>
#endif

namespace konami::utils {

OS PlatformUtils::getOS() {
#ifdef _WIN32
    return OS::Windows;
#elif defined(__APPLE__)
    return OS::macOS;
#elif defined(__linux__)
    return OS::Linux;
#else
    return OS::Unknown;
#endif
}

std::string PlatformUtils::getOSName() {
    switch (getOS()) {
        case OS::Windows: return "Windows";
        case OS::macOS: return "macOS";
        case OS::Linux: return "Linux";
        default: return "Unknown";
    }
}

std::string PlatformUtils::getOSVersion() {
#ifdef _WIN32
    OSVERSIONINFOW info;
    info.dwOSVersionInfoSize = sizeof(info);
    return "10+"; // Simplified
#elif defined(__APPLE__)
    return ""; // Would use sysctl
#else
    std::ifstream file("/etc/os-release");
    std::string line;
    while (std::getline(file, line)) {
        if (line.find("VERSION_ID=") == 0) {
            return line.substr(11);
        }
    }
    return "";
#endif
}

Architecture PlatformUtils::getArchitecture() {
#if defined(__x86_64__) || defined(_M_X64)
    return Architecture::x64;
#elif defined(__i386__) || defined(_M_IX86)
    return Architecture::x86;
#elif defined(__aarch64__) || defined(_M_ARM64)
    return Architecture::ARM64;
#elif defined(__arm__) || defined(_M_ARM)
    return Architecture::ARM;
#else
    return Architecture::Unknown;
#endif
}

std::string PlatformUtils::getArchitectureName() {
    switch (getArchitecture()) {
        case Architecture::x64: return "x86_64";
        case Architecture::x86: return "x86";
        case Architecture::ARM64: return "arm64";
        case Architecture::ARM: return "arm";
        default: return "unknown";
    }
}

bool PlatformUtils::is64Bit() {
    auto arch = getArchitecture();
    return arch == Architecture::x64 || arch == Architecture::ARM64;
}

SystemInfo PlatformUtils::getSystemInfo() {
    SystemInfo info;
    info.os = getOS();
    info.arch = getArchitecture();
    info.osName = getOSName();
    info.osVersion = getOSVersion();
    info.cpuCores = getCPUCores();
    info.totalMemory = getTotalMemory();
    info.availableMemory = getAvailableMemory();
    info.cpuModel = getCPUModel();
    info.username = getUsername();
    info.hostname = getHostname();
    return info;
}

int64_t PlatformUtils::getTotalMemory() {
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    return static_cast<int64_t>(status.ullTotalPhys);
#elif defined(__APPLE__)
    int mib[2] = {CTL_HW, HW_MEMSIZE};
    int64_t mem = 0;
    size_t len = sizeof(mem);
    sysctl(mib, 2, &mem, &len, nullptr, 0);
    return mem;
#else
    struct sysinfo si;
    sysinfo(&si);
    return static_cast<int64_t>(si.totalram) * si.mem_unit;
#endif
}

int64_t PlatformUtils::getAvailableMemory() {
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    return static_cast<int64_t>(status.ullAvailPhys);
#elif defined(__APPLE__)
    mach_port_t host = mach_host_self();
    vm_statistics64_data_t stats;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    host_statistics64(host, HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&stats), &count);
    return static_cast<int64_t>(stats.free_count) * vm_page_size;
#else
    struct sysinfo si;
    sysinfo(&si);
    return static_cast<int64_t>(si.freeram) * si.mem_unit;
#endif
}

int PlatformUtils::getCPUCores() {
    int cores = static_cast<int>(std::thread::hardware_concurrency());
    return cores > 0 ? cores : 1;
}

std::string PlatformUtils::getCPUModel() {
#ifdef __linux__
    std::ifstream file("/proc/cpuinfo");
    std::string line;
    while (std::getline(file, line)) {
        if (line.find("model name") == 0) {
            auto pos = line.find(':');
            if (pos != std::string::npos) return line.substr(pos + 2);
        }
    }
#endif
    return "Unknown";
}

double PlatformUtils::getCPUUsage() { return 0.0; /* TODO */ }

std::string PlatformUtils::getUsername() {
#ifdef _WIN32
    char buf[256];
    DWORD size = sizeof(buf);
    GetUserNameA(buf, &size);
    return buf;
#else
    const char* user = std::getenv("USER");
    return user ? user : "unknown";
#endif
}

std::string PlatformUtils::getHostname() {
#ifdef _WIN32
    char buf[256];
    DWORD size = sizeof(buf);
    GetComputerNameA(buf, &size);
    return buf;
#else
    char buf[256];
    gethostname(buf, sizeof(buf));
    return buf;
#endif
}

// -- GPU info (stubs) --

std::vector<GPUInfo> PlatformUtils::getGPUs() { return {}; }
GPUInfo PlatformUtils::getPrimaryGPU() { return {}; }
bool PlatformUtils::hasVulkanSupport() { return false; }
bool PlatformUtils::hasOpenGLSupport() { return true; }
bool PlatformUtils::hasMetalSupport() {
#ifdef __APPLE__
    return true;
#else
    return false;
#endif
}
bool PlatformUtils::hasDirectX12Support() {
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

// -- Java detection --

std::vector<JavaInfo> PlatformUtils::findJavaInstallations() {
    std::vector<JavaInfo> installs;
    std::vector<std::filesystem::path> searchPaths;

#ifdef _WIN32
    searchPaths.push_back("C:/Program Files/Java");
    searchPaths.push_back("C:/Program Files/Eclipse Adoptium");
    searchPaths.push_back("C:/Program Files/Zulu");
#elif defined(__APPLE__)
    searchPaths.push_back("/Library/Java/JavaVirtualMachines");
    searchPaths.push_back("/opt/homebrew/opt/openjdk");
#else
    searchPaths.push_back("/usr/lib/jvm");
    searchPaths.push_back("/usr/local/lib/jvm");
#endif

    for (const auto& sp : searchPaths) {
        if (!std::filesystem::exists(sp)) continue;
        for (const auto& entry : std::filesystem::directory_iterator(sp)) {
            if (entry.is_directory()) {
                auto javaBin = entry.path() / "bin" /
#ifdef _WIN32
                    "java.exe";
#else
                    "java";
#endif
                if (std::filesystem::exists(javaBin)) {
                    JavaInfo info;
                    info.path = javaBin;
                    info.version = entry.path().filename().string();
                    info.isValid = true;
                    info.is64Bit = is64Bit();
                    installs.push_back(info);
                }
            }
        }
    }

    return installs;
}

std::optional<JavaInfo> PlatformUtils::findJava(int /*minVersion*/) {
    auto installs = findJavaInstallations();
    if (!installs.empty()) return installs.front();
    return std::nullopt;
}

std::optional<JavaInfo> PlatformUtils::getJavaInfo(const std::filesystem::path& javaPath) {
    if (!std::filesystem::exists(javaPath)) return std::nullopt;
    JavaInfo info;
    info.path = javaPath;
    info.isValid = true;
    return info;
}

std::string PlatformUtils::getRecommendedJavaArgs(int64_t maxMemoryMB) {
    return "-Xms" + std::to_string(maxMemoryMB / 2) + "M -Xmx" + std::to_string(maxMemoryMB) + "M "
           "-XX:+UseG1GC -XX:+ParallelRefProcEnabled -XX:MaxGCPauseMillis=200";
}

bool PlatformUtils::isJavaValid(const std::filesystem::path& path) {
    return std::filesystem::exists(path);
}

// -- Process management --

int PlatformUtils::executeCommand(const std::string& command) { return std::system(command.c_str()); }

std::string PlatformUtils::executeCommandWithOutput(const std::string& command) {
    std::string result;
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (!pipe) return "";
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe)) result += buffer;
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return result;
}

bool PlatformUtils::startProcess(const std::string& /*path*/, const std::vector<std::string>& /*args*/) { return false; }
int PlatformUtils::startProcessAndWait(const std::string& /*path*/, const std::vector<std::string>& /*args*/) { return -1; }

bool PlatformUtils::isProcessRunning(int pid) {
#ifdef _WIN32
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!h) return false;
    DWORD exitCode;
    GetExitCodeProcess(h, &exitCode);
    CloseHandle(h);
    return exitCode == STILL_ACTIVE;
#else
    return kill(pid, 0) == 0;
#endif
}

bool PlatformUtils::killProcess(int pid) {
#ifdef _WIN32
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!h) return false;
    bool ok = TerminateProcess(h, 1) != 0;
    CloseHandle(h);
    return ok;
#else
    return kill(pid, SIGTERM) == 0;
#endif
}

int PlatformUtils::getCurrentProcessId() {
#ifdef _WIN32
    return static_cast<int>(GetCurrentProcessId());
#else
    return static_cast<int>(getpid());
#endif
}

// -- File associations --

bool PlatformUtils::openUrl(const std::string& url) {
#ifdef _WIN32
    return reinterpret_cast<intptr_t>(ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOW)) > 32;
#elif defined(__APPLE__)
    return std::system(("open " + url).c_str()) == 0;
#else
    return std::system(("xdg-open " + url).c_str()) == 0;
#endif
}

bool PlatformUtils::openFile(const std::filesystem::path& path) { return openUrl(path.string()); }
bool PlatformUtils::openFolder(const std::filesystem::path& path) { return openUrl(path.string()); }
bool PlatformUtils::showInExplorer(const std::filesystem::path& path) { return openFolder(path.parent_path()); }

// -- Environment --

std::optional<std::string> PlatformUtils::getEnv(const std::string& name) {
    const char* val = std::getenv(name.c_str());
    if (val) return std::string(val);
    return std::nullopt;
}

bool PlatformUtils::setEnv(const std::string& name, const std::string& value) {
#ifdef _WIN32
    return _putenv_s(name.c_str(), value.c_str()) == 0;
#else
    return setenv(name.c_str(), value.c_str(), 1) == 0;
#endif
}

std::string PlatformUtils::expandEnvVars(const std::string& str) { return str; /* TODO */ }

// -- Paths --

std::filesystem::path PlatformUtils::getHomeDirectory() {
    auto home = getEnv("HOME");
    if (!home) home = getEnv("USERPROFILE");
    return home ? std::filesystem::path(*home) : std::filesystem::current_path();
}

std::filesystem::path PlatformUtils::getAppDataDirectory() { return PathUtils::getAppDataPath(); }
std::filesystem::path PlatformUtils::getTempDirectory() { return std::filesystem::temp_directory_path(); }
std::filesystem::path PlatformUtils::getDocumentsDirectory() { return getHomeDirectory() / "Documents"; }
std::filesystem::path PlatformUtils::getDownloadsDirectory() { return getHomeDirectory() / "Downloads"; }
std::filesystem::path PlatformUtils::getDesktopDirectory() { return getHomeDirectory() / "Desktop"; }

std::filesystem::path PlatformUtils::getDefaultMinecraftDirectory() { return PathUtils::getMinecraftPath(); }
std::filesystem::path PlatformUtils::getMinecraftLauncherPath() { return std::filesystem::path(); }

// -- Native lib --

std::string PlatformUtils::getNativeLibrarySuffix() {
#ifdef _WIN32
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

std::string PlatformUtils::getNativeExecutableSuffix() {
#ifdef _WIN32
    return ".exe";
#else
    return "";
#endif
}

std::string PlatformUtils::getClasspathSeparator() {
#ifdef _WIN32
    return ";";
#else
    return ":";
#endif
}

// -- Display (stubs) --

std::pair<int, int> PlatformUtils::getPrimaryDisplayResolution() { return {1920, 1080}; }
std::vector<std::pair<int, int>> PlatformUtils::getAvailableResolutions() { return {{1920,1080},{1280,720},{854,480}}; }
double PlatformUtils::getDisplayScaleFactor() { return 1.0; }
bool PlatformUtils::isDarkModeEnabled() { return true; }

// -- Power (stubs) --

bool PlatformUtils::isOnBattery() { return false; }
int PlatformUtils::getBatteryPercentage() { return 100; }
bool PlatformUtils::isLaptop() { return false; }

// -- Notifications (stub) --

bool PlatformUtils::showNotification(const std::string& /*title*/, const std::string& /*message*/, const std::string& /*iconPath*/) { return false; }

// -- Clipboard (stubs) --

std::string PlatformUtils::getClipboardText() { return ""; }
bool PlatformUtils::setClipboardText(const std::string& /*text*/) { return false; }

// -- Single instance (stubs) --

bool PlatformUtils::acquireSingleInstanceLock(const std::string& /*appName*/) { return true; }
void PlatformUtils::releaseSingleInstanceLock() {}
bool PlatformUtils::isAnotherInstanceRunning(const std::string& /*appName*/) { return false; }

// -- MemoryMappedFile --

MemoryMappedFile::MemoryMappedFile() = default;

MemoryMappedFile::~MemoryMappedFile() { close(); }

bool MemoryMappedFile::open(const std::filesystem::path& /*path*/, bool /*readOnly*/) {
    // TODO: Platform-specific memory mapping
    return false;
}

void MemoryMappedFile::close() {
    m_data = nullptr;
    m_size = 0;
}

} // namespace konami::utils
