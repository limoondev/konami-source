# ╔══════════════════════════════════════════════════════════════════════════════╗
# ║                           KONAMI CLIENT                                       ║
# ║                  Revolutionary Minecraft Launcher                             ║
# ╚══════════════════════════════════════════════════════════════════════════════╝

cmake_minimum_required(VERSION 3.25)

# ══════════════════════════════════════════════════════════════════════════════
# Project Configuration
# ══════════════════════════════════════════════════════════════════════════════
project(KonamiClient
    VERSION 1.0.0
    DESCRIPTION "Revolutionary Minecraft Launcher with Premium UI"
    HOMEPAGE_URL "https://konami-client.dev"
    LANGUAGES CXX C
)

# Project metadata
set(KONAMI_AUTHOR "Konami Team")
set(KONAMI_LICENSE "MIT")
set(KONAMI_CODENAME "Phoenix")

# ══════════════════════════════════════════════════════════════════════════════
# CMake Policies & Configuration
# ══════════════════════════════════════════════════════════════════════════════
cmake_policy(SET CMP0135 NEW)  # FetchContent URL timestamps
cmake_policy(SET CMP0077 NEW)  # option() honors normal variables

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

# Module path for custom CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ══════════════════════════════════════════════════════════════════════════════
# C++23 Standard with Modern Features
# ══════════════════════════════════════════════════════════════════════════════
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Require C++23 features
set(REQUIRED_CXX_FEATURES
    cxx_std_23
    cxx_constexpr
    cxx_auto_type
    cxx_variadic_templates
    cxx_lambdas
    cxx_range_for
)

# ══════════════════════════════════════════════════════════════════════════════
# Build Configuration & Options
# ══════════════════════════════════════════════════════════════════════════════
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS 
        "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# Build options
option(KONAMI_BUILD_TESTS        "Build unit tests"                      OFF)
option(KONAMI_BUILD_BENCHMARKS   "Build performance benchmarks"          OFF)
option(KONAMI_BUILD_DOCS         "Build documentation"                   OFF)
option(KONAMI_ENABLE_LTO         "Enable Link Time Optimization"         OFF)
option(KONAMI_ENABLE_ASAN        "Enable AddressSanitizer"               OFF)
option(KONAMI_ENABLE_TSAN        "Enable ThreadSanitizer"                OFF)
option(KONAMI_ENABLE_UBSAN       "Enable UndefinedBehaviorSanitizer"     OFF)
option(KONAMI_ENABLE_MSAN        "Enable MemorySanitizer"                OFF)
option(KONAMI_ENABLE_PROFILING   "Enable CPU/GPU profiling"              OFF)
option(KONAMI_ENABLE_TRACY       "Enable Tracy profiler integration"     OFF)
option(KONAMI_STATIC_LINKING     "Prefer static linking"                 ON)
option(KONAMI_NATIVE_OPTIMIZATIONS "Enable CPU-specific optimizations"   OFF)
option(KONAMI_ENABLE_VULKAN      "Enable Vulkan renderer backend"        ON)
option(KONAMI_ENABLE_DIRECTX     "Enable DirectX 12 renderer (Windows)"  ON)
option(KONAMI_ENABLE_METAL       "Enable Metal renderer (macOS)"         ON)

# Output directories with organized structure
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Multi-config generator support
foreach(CONFIG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG_TYPE} CONFIG_TYPE_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} "${CMAKE_BINARY_DIR}/bin/${CONFIG_TYPE}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} "${CMAKE_BINARY_DIR}/lib/${CONFIG_TYPE}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} "${CMAKE_BINARY_DIR}/lib/${CONFIG_TYPE}")
endforeach()

# ══════════════════════════════════════════════════════════════════════════════
# Compiler Detection & Platform-Specific Configuration
# ══════════════════════════════════════════════════════════════════════════════
message(STATUS "")
message(STATUS "╔════════════════════════════════════════════════════════════════╗")
message(STATUS "║              Compiler & Platform Detection                     ║")
message(STATUS "╚════════════════════════════════════════════════════════════════╝")

# Detect compiler
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(KONAMI_COMPILER_GCC TRUE)
    message(STATUS "Compiler: GCC ${CMAKE_CXX_COMPILER_VERSION}")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if(MSVC)
        set(KONAMI_COMPILER_CLANGCL TRUE)
        message(STATUS "Compiler: Clang-CL ${CMAKE_CXX_COMPILER_VERSION}")
    else()
        set(KONAMI_COMPILER_CLANG TRUE)
        message(STATUS "Compiler: Clang ${CMAKE_CXX_COMPILER_VERSION}")
    endif()
elseif(MSVC)
    set(KONAMI_COMPILER_MSVC TRUE)
    message(STATUS "Compiler: MSVC ${MSVC_VERSION}")
endif()

# Detect platform
if(WIN32)
    set(KONAMI_PLATFORM_WINDOWS TRUE)
    set(KONAMI_PLATFORM_NAME "Windows")
    add_definitions(-DKONAMI_PLATFORM_WINDOWS -DNOMINMAX -DWIN32_LEAN_AND_MEAN -D_CRT_SECURE_NO_WARNINGS)
elseif(APPLE)
    set(KONAMI_PLATFORM_MACOS TRUE)
    set(KONAMI_PLATFORM_NAME "macOS")
    add_definitions(-DKONAMI_PLATFORM_MACOS)
elseif(UNIX)
    set(KONAMI_PLATFORM_LINUX TRUE)
    set(KONAMI_PLATFORM_NAME "Linux")
    add_definitions(-DKONAMI_PLATFORM_LINUX)
endif()

message(STATUS "Platform: ${KONAMI_PLATFORM_NAME} (${CMAKE_SYSTEM_PROCESSOR})")

# ══════════════════════════════════════════════════════════════════════════════
# Compiler Flags Configuration
# ══════════════════════════════════════════════════════════════════════════════

# Common warning flags
set(KONAMI_WARNING_FLAGS "")
set(KONAMI_RELEASE_FLAGS "")
set(KONAMI_DEBUG_FLAGS "")

if(KONAMI_COMPILER_GCC OR KONAMI_COMPILER_CLANG)
    # Warnings
    list(APPEND KONAMI_WARNING_FLAGS
        -Wall -Wextra -Wpedantic
        -Wcast-align -Wcast-qual -Wconversion
        -Wdouble-promotion -Wformat=2
        -Wimplicit-fallthrough=5
        -Wmisleading-indentation -Wmissing-include-dirs
        -Wnull-dereference -Wold-style-cast
        -Woverloaded-virtual -Wpointer-arith
        -Wshadow -Wsign-conversion
        -Wswitch-default -Wswitch-enum
        -Wundef -Wuninitialized -Wunreachable-code
        -Wunused -Wuseless-cast
        -Wzero-as-null-pointer-constant
        -Wno-unused-parameter  # Temporarily disable for development
    )
    
    # Release optimizations
    list(APPEND KONAMI_RELEASE_FLAGS
        -O3 -DNDEBUG
        -ffast-math
        -fno-signed-zeros -fno-trapping-math
        -funroll-loops -ftree-vectorize
        -fomit-frame-pointer
    )
    
    # Native optimizations (AVX2, SSE4.2, etc.)
    if(KONAMI_NATIVE_OPTIMIZATIONS)
        list(APPEND KONAMI_RELEASE_FLAGS -march=native -mtune=native)
    endif()
    
    # Debug flags
    list(APPEND KONAMI_DEBUG_FLAGS
        -g3 -ggdb -O0 -DDEBUG
        -fno-omit-frame-pointer
        -fno-optimize-sibling-calls
    )
    
    # Clang-specific
    if(KONAMI_COMPILER_CLANG)
        list(APPEND KONAMI_WARNING_FLAGS
            -Wno-c++98-compat -Wno-c++98-compat-pedantic
            -Wno-padded -Wno-weak-vtables
        )
    endif()
    
    # GCC-specific
    if(KONAMI_COMPILER_GCC)
        list(APPEND KONAMI_WARNING_FLAGS
            -Wlogical-op
        )
    endif()
    
elseif(KONAMI_COMPILER_MSVC OR KONAMI_COMPILER_CLANGCL)
    # MSVC warnings
    list(APPEND KONAMI_WARNING_FLAGS
        /W4 /permissive- /Zc:__cplusplus
        /wd4100  # unreferenced formal parameter
        /wd4201  # nonstandard extension: nameless struct/union
        /wd4324  # structure was padded due to alignment specifier
    )
    
    # Release optimizations
    list(APPEND KONAMI_RELEASE_FLAGS
        /O2 /Ob3 /GL /Gy /Gw /DNDEBUG
        /fp:fast /arch:AVX2
    )
    
    # Debug flags
    list(APPEND KONAMI_DEBUG_FLAGS
        /Od /Zi /RTC1 /DDEBUG
    )
endif()

# Link-time optimization
if(KONAMI_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT IPO_ERROR)
    
    if(IPO_SUPPORTED)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        message(STATUS "LTO: Enabled")
    else()
        message(STATUS "LTO: Not supported (${IPO_ERROR})")
    endif()
endif()

# ══════════════════════════════════════════════════════════════════════════════
# Sanitizers Configuration
# ══════════════════════════════════════════════════════════════════════════════
set(KONAMI_SANITIZER_FLAGS "")

if(KONAMI_ENABLE_ASAN AND NOT KONAMI_COMPILER_MSVC)
    message(STATUS "AddressSanitizer: Enabled")
    list(APPEND KONAMI_SANITIZER_FLAGS -fsanitize=address -fno-omit-frame-pointer)
endif()

if(KONAMI_ENABLE_TSAN AND NOT KONAMI_COMPILER_MSVC)
    message(STATUS "ThreadSanitizer: Enabled")
    list(APPEND KONAMI_SANITIZER_FLAGS -fsanitize=thread)
endif()

if(KONAMI_ENABLE_UBSAN AND NOT KONAMI_COMPILER_MSVC)
    message(STATUS "UndefinedBehaviorSanitizer: Enabled")
    list(APPEND KONAMI_SANITIZER_FLAGS -fsanitize=undefined)
endif()

if(KONAMI_ENABLE_MSAN AND KONAMI_COMPILER_CLANG)
    message(STATUS "MemorySanitizer: Enabled")
    list(APPEND KONAMI_SANITIZER_FLAGS -fsanitize=memory -fno-omit-frame-pointer)
endif()

# ══════════════════════════════════════════════════════════════════════════════
# Dependencies via FetchContent
# ══════════════════════════════════════════════════════════════════════════════
message(STATUS "")
message(STATUS "╔════════════════════════════════════════════════════════════════╗")
message(STATUS "║                   Fetching Dependencies                        ║")
message(STATUS "╚════════════════════════════════════════════════════════════════╝")

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# ─────────────────────────────────────────────────────────────────────────────
# Slint UI Framework - The Heart of Our UI
# ─────────────────────────────────────────────────────────────────────────────
message(STATUS "Fetching Slint UI Framework...")
FetchContent_Declare(
    Slint
    GIT_REPOSITORY https://github.com/slint-ui/slint.git
    GIT_TAG        v1.8.0
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

# Slint configuration
set(SLINT_FEATURE_BACKEND_WINIT ON CACHE BOOL "" FORCE)
set(SLINT_FEATURE_RENDERER_SKIA ON CACHE BOOL "" FORCE)
set(SLINT_FEATURE_RENDERER_FEMTOVG ON CACHE BOOL "" FORCE)

if(KONAMI_ENABLE_VULKAN)
    set(SLINT_FEATURE_RENDERER_SKIA_VULKAN ON CACHE BOOL "" FORCE)
endif()

if(KONAMI_PLATFORM_WINDOWS AND KONAMI_ENABLE_DIRECTX)
    set(SLINT_FEATURE_RENDERER_SKIA_D3D ON CACHE BOOL "" FORCE)
endif()

if(KONAMI_PLATFORM_MACOS AND KONAMI_ENABLE_METAL)
    set(SLINT_FEATURE_RENDERER_SKIA_METAL ON CACHE BOOL "" FORCE)
endif()

FetchContent_MakeAvailable(Slint)

# ─────────────────────────────────────────────────────────────────────────────
# JSON Library - nlohmann/json
# ─────────────────────────────────────────────────────────────────────────────
message(STATUS "Fetching nlohmann/json...")
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(json)

# ─────────────────────────────────────────────────────────────────────────────
# HTTP Client - cpr (C++ Requests)
# ─────────────────────────────────────────────────────────────────────────────
message(STATUS "Fetching cpr HTTP library...")
FetchContent_Declare(
    cpr
    GIT_REPOSITORY https://github.com/libcpr/cpr.git
    GIT_TAG        1.10.5
    GIT_SHALLOW    TRUE
)
set(CPR_USE_SYSTEM_CURL OFF CACHE BOOL "" FORCE)
set(CPR_ENABLE_SSL ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CPR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(cpr)

# ─────────────────────────────────────────────────────────────────────────────
# Logging - spdlog (blazing fast)
# ─────────────────────────────────────────────────────────────────────────────
message(STATUS "Fetching spdlog...")
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
    GIT_SHALLOW    TRUE
)
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)

# ─────────────────────────────────────────────────────────────────────────────
# Async Runtime - Asio (standalone, no Boost)
# ─────────────────────────────────────────────────────────────────────────────
message(STATUS "Fetching Asio...")
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-30-2
    GIT_SHALLOW    TRUE
)
set(ASIO_SEPARATE_COMPILATION OFF CACHE BOOL "" FORCE)
set(ASIO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(asio)

# ─────────────────────────────────────────────────────────────────────────────
# Compression - ZLIB, LZ4, ZSTD
# ─────────────────────────────────────────────────────────────────────────────
message(STATUS "Fetching compression libraries...")

# ZLIB with proper alias handling
FetchContent_Declare(
    zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG        v1.3.1
    GIT_SHALLOW    TRUE
)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zlib)

# Ensure ZLIB alias exists
if(NOT TARGET ZLIB::ZLIB)
    if(TARGET zlib)
        add_library(ZLIB::ZLIB ALIAS zlib)
    elseif(TARGET zlibstatic)
        add_library(ZLIB::ZLIB ALIAS zlibstatic)
    endif()
endif()

FetchContent_Declare(
    lz4
    GIT_REPOSITORY https://github.com/lz4/lz4.git
    GIT_TAG        v1.9.4
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  build/cmake
)
set(LZ4_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(LZ4_BUILD_LEGACY_LZ4C OFF CACHE BOOL "" FORCE)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW CACHE STRING "" FORCE)
FetchContent_MakeAvailable(lz4)

FetchContent_Declare(
    zstd
    GIT_REPOSITORY https://github.com/facebook/zstd.git
    GIT_TAG        v1.5.6
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  build/cmake
)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zstd)

# ─────────────────────────────────────────────────────────────────────────────
# Cryptography - OpenSSL
# ─────────────────────────────────────────────────────────────────────────────
find_package(OpenSSL REQUIRED)
message(STATUS "OpenSSL: ${OPENSSL_VERSION}")

# ─────────────────────────────────────────────────────────────────────────────
# Image Processing - stb
# ─────────────────────────────────────────────────────────────────────────────
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(stb)

# ─────────────────────────────────────────────────────────────────────────────
# UUID Generation - stduuid
# ─────────────────────────────────────────────────────────────────────────────
FetchContent_Declare(
    stduuid
    GIT_REPOSITORY https://github.com/mariusbancila/stduuid.git
    GIT_TAG        v1.2.3
    GIT_SHALLOW    TRUE
)
set(UUID_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(stduuid)

# ─────────────────────────────────────────────────────────────────────────────
# Date/Time - Howard Hinnant's date library
# ─────────────────────────────────────────────────────────────────────────────
FetchContent_Declare(
    date
    GIT_REPOSITORY https://github.com/HowardHinnant/date.git
    GIT_TAG        v3.0.1
    GIT_SHALLOW    TRUE
)
set(BUILD_TZ_LIB ON CACHE BOOL "" FORCE)
set(USE_SYSTEM_TZ_DB ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(date)

# ─────────────────────────────────────────────────────────────────────────────
# Reflection/Serialization - magic_enum
# ─────────────────────────────────────────────────────────────────────────────
FetchContent_Declare(
    magic_enum
    GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
    GIT_TAG        v0.9.6
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(magic_enum)

# ─────────────────────────────────────────────────────────────────────────────
# CLI Argument Parser - CLI11
# ─────────────────────────────────────────────────────────────────────────────
FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG        v2.4.2
    GIT_SHALLOW    TRUE
)
set(CLI11_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(CLI11_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CLI11_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(CLI11)

# ─────────────────────────────────────────────────────────────────────────────
# Tracy Profiler (optional)
# ─────────────────────────────────────────────────────────────────────────────
if(KONAMI_ENABLE_TRACY)
    FetchContent_Declare(
        tracy
        GIT_REPOSITORY https://github.com/wolfpld/tracy.git
        GIT_TAG        v0.11.0
        GIT_SHALLOW    TRUE
    )
    set(TRACY_ENABLE ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(tracy)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Testing - Catch2
# ─────────────────────────────────────────────────────────────────────────────
if(KONAMI_BUILD_TESTS)
    message(STATUS "Fetching Catch2...")
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.7.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Benchmarking - Google Benchmark (optional)
# ─────────────────────────────────────────────────────────────────────────────
if(KONAMI_BUILD_BENCHMARKS)
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG        v1.8.5
        GIT_SHALLOW    TRUE
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(benchmark)
endif()

# ══════════════════════════════════════════════════════════════════════════════
# Source Files Organization
# ══════════════════════════════════════════════════════════════════════════════
message(STATUS "")
message(STATUS "╔════════════════════════════════════════════════════════════════╗")
message(STATUS "║                   Configuring Source Files                     ║")
message(STATUS "╚════════════════════════════════════════════════════════════════╝")

# Core Engine
set(KONAMI_CORE_SOURCES
    src/core/Application.cpp
    src/core/Config.cpp
    src/core/EventBus.cpp
    src/core/ThreadPool.cpp
    src/core/TaskScheduler.cpp
    src/core/PluginManager.cpp
)

# Logging System
set(KONAMI_LOGGING_SOURCES
    src/core/logging/Logger.cpp
    src/core/logging/LogFormatter.cpp
    src/core/logging/CrashReporter.cpp
)

# Authentication
set(KONAMI_AUTH_SOURCES
    src/auth/AuthManager.cpp
    src/auth/MicrosoftOAuth.cpp
    src/auth/XboxLiveAuth.cpp
    src/auth/MinecraftAuth.cpp
    src/auth/TokenStorage.cpp
    src/auth/TokenRefresher.cpp
    src/auth/Encryption.cpp
    src/auth/KeychainManager.cpp
)

# Download System
set(KONAMI_DOWNLOAD_SOURCES
    src/download/DownloadManager.cpp
    src/download/DownloadQueue.cpp
    src/download/DownloadTask.cpp
    src/download/RateLimiter.cpp
    src/download/RetryHandler.cpp
    src/download/IntegrityChecker.cpp
    src/download/CDNSelector.cpp
)

# API Integrations
set(KONAMI_API_SOURCES
    src/api/MojangAPI.cpp
    src/api/LaunchermMetaAPI.cpp
    src/api/ForgeAPI.cpp
    src/api/FabricAPI.cpp
    src/api/QuiltAPI.cpp
    src/api/NeoForgeAPI.cpp
    src/api/ModrinthAPI.cpp
    src/api/CurseForgeAPI.cpp
    src/api/HangarAPI.cpp
    src/api/GitHubAPI.cpp
)

# Game Management
set(KONAMI_GAME_SOURCES
    src/game/GameLauncher.cpp
    src/game/ProcessManager.cpp
    src/game/JavaManager.cpp
    src/game/JavaDetector.cpp
    src/game/JavaInstaller.cpp
    src/game/VersionManager.cpp
    src/game/AssetManager.cpp
    src/game/LibraryManager.cpp
    src/game/NativesExtractor.cpp
    src/game/ArgumentBuilder.cpp
    src/game/GameWatcher.cpp
)

# Profile Management
set(KONAMI_PROFILE_SOURCES
    src/profile/ProfileManager.cpp
    src/profile/ProfileValidator.cpp
    src/profile/ProfileExporter.cpp
    src/profile/ProfileImporter.cpp
    src/profile/CloudSync.cpp
)

# Mod Management
set(KONAMI_MOD_SOURCES
    src/mods/ModManager.cpp
    src/mods/ModLoader.cpp
    src/mods/ModInstaller.cpp
    src/mods/ModUpdater.cpp
    src/mods/DependencyResolver.cpp
    src/mods/ConflictResolver.cpp
    src/mods/ModpackManager.cpp
    src/mods/ModpackExporter.cpp
)

# Skin System
set(KONAMI_SKIN_SOURCES
    src/skin/SkinManager.cpp
    src/skin/SkinRenderer.cpp
    src/skin/SkinEditor.cpp
    src/skin/SkinUploader.cpp
    src/skin/CapeManager.cpp
    src/skin/SkinCache.cpp
)

# Instance Management
set(KONAMI_INSTANCE_SOURCES
    src/instance/InstanceManager.cpp
    src/instance/InstanceCreator.cpp
    src/instance/InstanceMigrator.cpp
    src/instance/WorldManager.cpp
    src/instance/ScreenshotManager.cpp
    src/instance/ResourcePackManager.cpp
    src/instance/ShaderPackManager.cpp
)

# Cache System
set(KONAMI_CACHE_SOURCES
    src/cache/CacheManager.cpp
    src/cache/DiskCache.cpp
    src/cache/MemoryCache.cpp
    src/cache/CacheEviction.cpp
)

# Utility Functions
set(KONAMI_UTIL_SOURCES
    src/utils/FileSystem.cpp
    src/utils/StringUtils.cpp
    src/utils/HashUtils.cpp
    src/utils/CompressionUtils.cpp
    src/utils/NetworkUtils.cpp
    src/utils/PlatformUtils.cpp
    src/utils/JsonUtils.cpp
    src/utils/TimeUtils.cpp
    src/utils/UUIDUtils.cpp
    src/utils/Base64.cpp
    src/utils/Validation.cpp
)

# Data Models
set(KONAMI_MODEL_SOURCES
    src/models/Account.cpp
    src/models/Profile.cpp
    src/models/Version.cpp
    src/models/Mod.cpp
    src/models/Modpack.cpp
    src/models/Instance.cpp
    src/models/Skin.cpp
    src/models/Settings.cpp
    src/models/News.cpp
)

# UI Bindings (Slint <-> C++)
set(KONAMI_UI_BINDING_SOURCES
    src/ui/bridge/UIBridge.cpp
    src/ui/bridge/MainWindowBridge.cpp
    src/ui/bridge/AuthBridge.cpp
    src/ui/bridge/ProfileBridge.cpp
    src/ui/bridge/ModsBridge.cpp
    src/ui/bridge/SkinBridge.cpp
    src/ui/bridge/SettingsBridge.cpp
    src/ui/bridge/DownloadBridge.cpp
    src/ui/bridge/ConsoleBridge.cpp
    src/ui/bridge/ThemeBridge.cpp
    src/ui/bridge/NotificationBridge.cpp
)

# Platform-specific sources
if(KONAMI_PLATFORM_WINDOWS)
    list(APPEND KONAMI_UTIL_SOURCES
        src/platform/windows/WinRegistry.cpp
        src/platform/windows/WinUtils.cpp
        src/platform/windows/WinTrayIcon.cpp
    )
elseif(KONAMI_PLATFORM_MACOS)
    list(APPEND KONAMI_UTIL_SOURCES
        src/platform/macos/MacUtils.mm
        src/platform/macos/MacKeychain.mm
        src/platform/macos/MacNotifications.mm
    )
elseif(KONAMI_PLATFORM_LINUX)
    list(APPEND KONAMI_UTIL_SOURCES
        src/platform/linux/LinuxUtils.cpp
        src/platform/linux/LinuxSecretService.cpp
        src/platform/linux/LinuxNotifications.cpp
    )
endif()

# Combine all sources
set(KONAMI_ALL_SOURCES
    ${KONAMI_CORE_SOURCES}
    ${KONAMI_LOGGING_SOURCES}
    ${KONAMI_AUTH_SOURCES}
    ${KONAMI_DOWNLOAD_SOURCES}
    ${KONAMI_API_SOURCES}
    ${KONAMI_GAME_SOURCES}
    ${KONAMI_PROFILE_SOURCES}
    ${KONAMI_MOD_SOURCES}
    ${KONAMI_SKIN_SOURCES}
    ${KONAMI_INSTANCE_SOURCES}
    ${KONAMI_CACHE_SOURCES}
    ${KONAMI_UTIL_SOURCES}
    ${KONAMI_MODEL_SOURCES}
    ${KONAMI_UI_BINDING_SOURCES}
)

# ══════════════════════════════════════════════════════════════════════════════
# Slint UI Files
# ══════════════════════════════════════════════════════════════════════════════

# Theme System
set(KONAMI_SLINT_THEMES
    src/ui/themes/base.slint
    src/ui/themes/cyberpunk.slint
    src/ui/themes/glassmorphism.slint
    src/ui/themes/aurora.slint
    src/ui/themes/material.slint
    src/ui/themes/retro-arcade.slint
    src/ui/themes/nord.slint
    src/ui/themes/dracula.slint
)

# Component Library
set(KONAMI_SLINT_COMPONENTS
    src/ui/components/button.slint
    src/ui/components/input.slint
    src/ui/components/card.slint
    src/ui/components/modal.slint
    src/ui/components/dropdown.slint
    src/ui/components/toggle.slint
    src/ui/components/slider.slint
    src/ui/components/progress.slint
    src/ui/components/tabs.slint
    src/ui/components/sidebar.slint
    src/ui/components/titlebar.slint
    src/ui/components/avatar.slint
    src/ui/components/tooltip.slint
    src/ui/components/notification.slint
    src/ui/components/search.slint
    src/ui/components/tag.slint
    src/ui/components/badge.slint
    src/ui/components/skeleton.slint
    src/ui/components/scrollbar.slint
    src/ui/components/context-menu.slint
    src/ui/components/carousel.slint
    src/ui/components/stepper.slint
    src/ui/components/rating.slint
    src/ui/components/color-picker.slint
    src/ui/components/file-picker.slint
    src/ui/components/image-viewer.slint
    src/ui/components/code-block.slint
    src/ui/components/markdown.slint
)

# Page Views
set(KONAMI_SLINT_PAGES
    src/ui/pages/home.slint
    src/ui/pages/library.slint
    src/ui/pages/profiles.slint
    src/ui/pages/mods-browser.slint
    src/ui/pages/mods-installed.slint
    src/ui/pages/modpacks.slint
    src/ui/pages/skins.slint
    src/ui/pages/settings-general.slint
    src/ui/pages/settings-java.slint
    src/ui/pages/settings-accounts.slint
    src/ui/pages/settings-appearance.slint
    src/ui/pages/settings-advanced.slint
    src/ui/pages/console.slint
    src/ui/pages/login.slint
    src/ui/pages/downloads.slint
    src/ui/pages/news.slint
    src/ui/pages/changelog.slint
    src/ui/pages/about.slint
)

# Dialogs
set(KONAMI_SLINT_DIALOGS
    src/ui/dialogs/create-profile.slint
    src/ui/dialogs/edit-profile.slint
    src/ui/dialogs/install-java.slint
    src/ui/dialogs/add-account.slint
    src/ui/dialogs/confirm.slint
    src/ui/dialogs/error.slint
    src/ui/dialogs/crash-report.slint
    src/ui/dialogs/update-available.slint
    src/ui/dialogs/mod-details.slint
    src/ui/dialogs/skin-preview.slint
)

# Animations
set(KONAMI_SLINT_ANIMATIONS
    src/ui/animations/transitions.slint
    src/ui/animations/effects.slint
    src/ui/animations/particles.slint
    src/ui/animations/loaders.slint
)

# Main Entry
set(KONAMI_SLINT_MAIN
    src/ui/main-window.slint
)

# Combine all Slint files
set(KONAMI_SLINT_FILES
    ${KONAMI_SLINT_MAIN}
    ${KONAMI_SLINT_THEMES}
    ${KONAMI_SLINT_COMPONENTS}
    ${KONAMI_SLINT_PAGES}
    ${KONAMI_SLINT_DIALOGS}
    ${KONAMI_SLINT_ANIMATIONS}
)

# ══════════════════════════════════════════════════════════════════════════════
# Main Executable Target
# ══════════════════════════════════════════════════════════════════════════════
message(STATUS "")
message(STATUS "╔════════════════════════════════════════════════════════════════╗")
message(STATUS "║                   Building Main Executable                     ║")
message(STATUS "╚════════════════════════════════════════════════════════════════╝")

# Create executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    ${KONAMI_ALL_SOURCES}
)

# Windows-specific: Set as GUI application
if(KONAMI_PLATFORM_WINDOWS)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
    
    # Add Windows resource file (icon, version info)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/windows/konami.rc")
        target_sources(${PROJECT_NAME} PRIVATE
            resources/windows/konami.rc
        )
    endif()
endif()

# macOS-specific: Create app bundle
if(KONAMI_PLATFORM_MACOS)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Konami Client"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "dev.konami.client"
        MACOSX_BUNDLE_ICON_FILE "konami.icns"
    )
endif()

# Slint UI sources
slint_target_sources(${PROJECT_NAME} ${KONAMI_SLINT_FILES})

# C++ features
target_compile_features(${PROJECT_NAME} PUBLIC ${REQUIRED_CXX_FEATURES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Compiler flags
target_compile_options(${PROJECT_NAME} PRIVATE
    ${KONAMI_WARNING_FLAGS}
    ${KONAMI_SANITIZER_FLAGS}
    $<$<CONFIG:Debug>:${KONAMI_DEBUG_FLAGS}>
    $<$<CONFIG:Release>:${KONAMI_RELEASE_FLAGS}>
    $<$<CONFIG:RelWithDebInfo>:${KONAMI_RELEASE_FLAGS} -g>
)

# Linker flags
if(KONAMI_SANITIZER_FLAGS)
    target_link_options(${PROJECT_NAME} PRIVATE ${KONAMI_SANITIZER_FLAGS})
endif()

# ══════════════════════════════════════════════════════════════════════════════
# Link Libraries
# ══════════════════════════════════════════════════════════════════════════════

# Core dependencies
target_link_libraries(${PROJECT_NAME} PRIVATE
    Slint::Slint
    nlohmann_json::nlohmann_json
    cpr::cpr
    spdlog::spdlog
    asio::asio
    ZLIB::ZLIB
    lz4::lz4
    zstd::libzstd_static
    OpenSSL::SSL
    OpenSSL::Crypto
    stb::stb
    stduuid::stduuid
    date::date
    date::date-tz
    magic_enum::magic_enum
    CLI11::CLI11
)

# Optional: Tracy profiler
if(KONAMI_ENABLE_TRACY)
    target_link_libraries(${PROJECT_NAME} PRIVATE Tracy::TracyClient)
    target_compile_definitions(${PROJECT_NAME} PRIVATE KONAMI_ENABLE_TRACY)
endif()

# Platform-specific libraries
if(KONAMI_PLATFORM_WINDOWS)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ws2_32
        crypt32
        bcrypt
        userenv
        shell32
        advapi32
        ole32
        uuid
        dwmapi
        uxtheme
    )
elseif(KONAMI_PLATFORM_MACOS)
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    find_library(SECURITY_LIBRARY Security REQUIRED)
    find_library(IOKIT_LIBRARY IOKit REQUIRED)
    find_library(FOUNDATION_LIBRARY Foundation REQUIRED)
    find_library(APPKIT_LIBRARY AppKit REQUIRED)
    find_library(COREFOUNDATION_LIBRARY CoreFoundation REQUIRED)
    
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${COCOA_LIBRARY}
        ${SECURITY_LIBRARY}
        ${IOKIT_LIBRARY}
        ${FOUNDATION_LIBRARY}
        ${APPKIT_LIBRARY}
        ${COREFOUNDATION_LIBRARY}
        "-framework Metal"
        "-framework QuartzCore"
    )
elseif(KONAMI_PLATFORM_LINUX)
    find_package(Threads REQUIRED)
    find_package(PkgConfig REQUIRED)
    
    pkg_check_modules(LIBSECRET libsecret-1)
    pkg_check_modules(DBUS dbus-1)
    
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Threads::Threads
        ${CMAKE_DL_LIBS}
    )
    
    if(LIBSECRET_FOUND)
        target_include_directories(${PROJECT_NAME} PRIVATE ${LIBSECRET_INCLUDE_DIRS})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBSECRET_LIBRARIES})
        target_compile_definitions(${PROJECT_NAME} PRIVATE KONAMI_HAS_LIBSECRET)
    endif()
    
    if(DBUS_FOUND)
        target_include_directories(${PROJECT_NAME} PRIVATE ${DBUS_INCLUDE_DIRS})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${DBUS_LIBRARIES})
    endif()
endif()

# ══════════════════════════════════════════════════════════════════════════════
# Unit Tests
# ══════════════════════════════════════════════════════════════════════════════
if(KONAMI_BUILD_TESTS)
    message(STATUS "Building unit tests...")
    enable_testing()
    
    add_executable(KonamiTests
        tests/main_test.cpp
        tests/test_auth.cpp
        tests/test_download.cpp
        tests/test_profile.cpp
        tests/test_mods.cpp
        tests/test_encryption.cpp
        tests/test_compression.cpp
        tests/test_cache.cpp
        tests/test_utils.cpp
        tests/test_models.cpp
        tests/test_api.cpp
        ${KONAMI_ALL_SOURCES}
    )
    
    slint_target_sources(KonamiTests ${KONAMI_SLINT_FILES})
    
    target_include_directories(KonamiTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )
    
    target_compile_features(KonamiTests PUBLIC ${REQUIRED_CXX_FEATURES})
    
    target_compile_options(KonamiTests PRIVATE ${KONAMI_WARNING_FLAGS})
    
    target_link_libraries(KonamiTests PRIVATE
        Catch2::Catch2WithMain
        Slint::Slint
        nlohmann_json::nlohmann_json
        cpr::cpr
        spdlog::spdlog
        asio::asio
        ZLIB::ZLIB
        lz4::lz4
        zstd::libzstd_static
        OpenSSL::SSL
        OpenSSL::Crypto
        stb::stb
        stduuid::stduuid
        date::date
        date::date-tz
        magic_enum::magic_enum
    )
    
    # Platform libraries for tests
    if(KONAMI_PLATFORM_WINDOWS)
        target_link_libraries(KonamiTests PRIVATE ws2_32 crypt32 bcrypt userenv shell32 advapi32)
    elseif(KONAMI_PLATFORM_MACOS)
        target_link_libraries(KonamiTests PRIVATE ${COCOA_LIBRARY} ${SECURITY_LIBRARY} ${IOKIT_LIBRARY})
    elseif(KONAMI_PLATFORM_LINUX)
        target_link_libraries(KonamiTests PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
    endif()
    
    include(CTest)
    include(Catch)
    catch_discover_tests(KonamiTests)
endif()

# ══════════════════════════════════════════════════════════════════════════════
# Benchmarks
# ══════════════════════════════════════════════════════════════════════════════
if(KONAMI_BUILD_BENCHMARKS)
    message(STATUS "Building benchmarks...")
    
    add_executable(KonamiBenchmarks
        benchmarks/main_benchmark.cpp
        benchmarks/bench_download.cpp
        benchmarks/bench_compression.cpp
        benchmarks/bench_cache.cpp
        benchmarks/bench_json.cpp
        ${KONAMI_ALL_SOURCES}
    )
    
    slint_target_sources(KonamiBenchmarks ${KONAMI_SLINT_FILES})
    
    target_include_directories(KonamiBenchmarks PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    
    target_link_libraries(KonamiBenchmarks PRIVATE
        benchmark::benchmark
        benchmark::benchmark_main
        Slint::Slint
        nlohmann_json::nlohmann_json
        cpr::cpr
        spdlog::spdlog
        asio::asio
        ZLIB::ZLIB
        lz4::lz4
        zstd::libzstd_static
        OpenSSL::SSL
        OpenSSL::Crypto
        stb::stb
        stduuid::stduuid
        date::date
        date::date-tz
        magic_enum::magic_enum
    )
endif()

# ══════════════════════════════════════════════════════════════════════════════
# Installation
# ══════════════════════════════════════════════════════════════════════════════
message(STATUS "")
message(STATUS "╔════════════════════════════════════════════════════════════════╗")
message(STATUS "║                   Installation Configuration                   ║")
message(STATUS "╚════════════════════════════════════════════════════════════════╝")

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install resources
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources")
    install(DIRECTORY resources/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}
        OPTIONAL
    )
endif()

# Install license and docs
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    install(FILES LICENSE
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
        OPTIONAL
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    install(FILES README.md
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
        OPTIONAL
    )
endif()

# Desktop file for Linux
if(KONAMI_PLATFORM_LINUX AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/linux/konami-client.desktop")
    install(FILES resources/linux/konami-client.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
        OPTIONAL
    )
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/konami-256.png")
        install(FILES resources/icons/konami-256.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
            RENAME konami-client.png
            OPTIONAL
        )
    endif()
endif()

# ══════════════════════════════════════════════════════════════════════════════
# CPack Packaging
# ══════════════════════════════════════════════════════════════════════════════
set(CPACK_PACKAGE_NAME "KonamiClient")
set(CPACK_PACKAGE_VENDOR "${KONAMI_AUTHOR}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Revolutionary Minecraft Launcher with Premium UI")
set(CPACK_PACKAGE_DESCRIPTION "Konami Client is a next-generation Minecraft launcher featuring cutting-edge UI/UX design, blazing-fast performance, comprehensive mod management, and enterprise-grade security.")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")

# Package naming
set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-${KONAMI_PLATFORM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

if(KONAMI_PLATFORM_WINDOWS)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME "Konami Client")
    set(CPACK_NSIS_PACKAGE_NAME "Konami Client")
    set(CPACK_NSIS_HELP_LINK "${PROJECT_HOMEPAGE_URL}")
    set(CPACK_NSIS_URL_INFO_ABOUT "${PROJECT_HOMEPAGE_URL}")
    set(CPACK_NSIS_CONTACT "support@konami-client.dev")
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
elseif(KONAMI_PLATFORM_MACOS)
    set(CPACK_GENERATOR "DragNDrop;ZIP")
    set(CPACK_DMG_VOLUME_NAME "Konami Client")
    set(CPACK_DMG_FORMAT "UDBZ")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)

# ══════════════════════════════════════════════════════════════════════════════
# Build Summary
# ══════════════════════════════════════════════════════════════════════════════
message(STATUS "")
message(STATUS "╔════════════════════════════════════════════════════════════════════════════╗")
message(STATUS "║                          BUILD CONFIGURATION SUMMARY                       ║")
message(STATUS "╠════════════════════════════════════════════════════════════════════════════╣")
message(STATUS "║  Project:         ${PROJECT_NAME} v${PROJECT_VERSION} (${KONAMI_CODENAME})")
message(STATUS "║  Platform:        ${KONAMI_PLATFORM_NAME} (${CMAKE_SYSTEM_PROCESSOR})")
message(STATUS "║  Compiler:        ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "║  C++ Standard:    C++${CMAKE_CXX_STANDARD}")
message(STATUS "║  Build Type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "╠════════════════════════════════════════════════════════════════════════════╣")
message(STATUS "║  Options:")
message(STATUS "║    - LTO:               ${KONAMI_ENABLE_LTO}")
message(STATUS "║    - Tests:             ${KONAMI_BUILD_TESTS}")
message(STATUS "║    - Benchmarks:        ${KONAMI_BUILD_BENCHMARKS}")
message(STATUS "║    - Tracy Profiler:    ${KONAMI_ENABLE_TRACY}")
message(STATUS "║    - Native Opts:       ${KONAMI_NATIVE_OPTIMIZATIONS}")
if(KONAMI_ENABLE_VULKAN)
message(STATUS "║    - Vulkan Backend:    ON")
endif()
if(KONAMI_PLATFORM_WINDOWS AND KONAMI_ENABLE_DIRECTX)
message(STATUS "║    - DirectX Backend:   ON")
endif()
if(KONAMI_PLATFORM_MACOS AND KONAMI_ENABLE_METAL)
message(STATUS "║    - Metal Backend:     ON")
endif()
message(STATUS "╠════════════════════════════════════════════════════════════════════════════╣")
message(STATUS "║  Sanitizers:")
message(STATUS "║    - AddressSanitizer:  ${KONAMI_ENABLE_ASAN}")
message(STATUS "║    - ThreadSanitizer:   ${KONAMI_ENABLE_TSAN}")
message(STATUS "║    - UBSanitizer:       ${KONAMI_ENABLE_UBSAN}")
message(STATUS "╠════════════════════════════════════════════════════════════════════════════╣")
message(STATUS "║  Output Directories:")
message(STATUS "║    - Binary:    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "║    - Library:   ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
message(STATUS "╚════════════════════════════════════════════════════════════════════════════╝")
message(STATUS "")

# Final validation
message(STATUS "")
message(STATUS "╔════════════════════════════════════════════════════════════════╗")
message(STATUS "║                   BUILD SYSTEM READY                           ║")
message(STATUS "╠════════════════════════════════════════════════════════════════╣")
message(STATUS "║  Supported Generators:                                         ║")
message(STATUS "║    - Ninja              (Recommended)                         ║")
message(STATUS "║    - Visual Studio 17 2022                                    ║")
message(STATUS "║    - Visual Studio 16 2019                                    ║")
message(STATUS "║    - Unix Makefiles                                           ║")
message(STATUS "╚════════════════════════════════════════════════════════════════╝")
message(STATUS "")