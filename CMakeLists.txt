# ============================================================================
# KONAMI CLIENT - Revolutionary Minecraft Launcher
# ============================================================================
cmake_minimum_required(VERSION 3.25)

# ============================================================================
# Project Configuration
# ============================================================================
project(KonamiClient
    VERSION 1.0.0
    DESCRIPTION "Revolutionary Minecraft Launcher with Premium UI"
    HOMEPAGE_URL "https://konami-client.dev"
    LANGUAGES CXX C
)

set(KONAMI_AUTHOR "Konami Team")
set(KONAMI_LICENSE "MIT")
set(KONAMI_CODENAME "Phoenix")

# ============================================================================
# CMake Policies & Configuration
# ============================================================================
cmake_policy(SET CMP0135 NEW)
cmake_policy(SET CMP0077 NEW)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ============================================================================
# C++ Standard
# ============================================================================
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# Build Configuration & Options
# ============================================================================
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

option(KONAMI_BUILD_TESTS          "Build unit tests"                      OFF)
option(KONAMI_BUILD_BENCHMARKS     "Build performance benchmarks"          OFF)
option(KONAMI_BUILD_DOCS           "Build documentation"                   OFF)
option(KONAMI_ENABLE_LTO           "Enable Link Time Optimization"         OFF)
option(KONAMI_ENABLE_ASAN          "Enable AddressSanitizer"               OFF)
option(KONAMI_ENABLE_TSAN          "Enable ThreadSanitizer"                OFF)
option(KONAMI_ENABLE_UBSAN         "Enable UndefinedBehaviorSanitizer"     OFF)
option(KONAMI_ENABLE_MSAN          "Enable MemorySanitizer"                OFF)
option(KONAMI_ENABLE_TRACY         "Enable Tracy profiler integration"     OFF)
option(KONAMI_STATIC_LINKING       "Prefer static linking"                 ON)
option(KONAMI_NATIVE_OPTIMIZATIONS "Enable CPU-specific optimizations"     OFF)
option(KONAMI_ENABLE_VULKAN        "Enable Vulkan renderer backend"        ON)
option(KONAMI_ENABLE_DIRECTX       "Enable DirectX 12 renderer (Windows)"  ON)
option(KONAMI_ENABLE_METAL         "Enable Metal renderer (macOS)"         ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

foreach(CONFIG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG_TYPE} CONFIG_TYPE_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} "${CMAKE_BINARY_DIR}/bin/${CONFIG_TYPE}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} "${CMAKE_BINARY_DIR}/lib/${CONFIG_TYPE}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} "${CMAKE_BINARY_DIR}/lib/${CONFIG_TYPE}")
endforeach()

# ============================================================================
# Compiler Detection & Platform Configuration
# ============================================================================
message(STATUS "")
message(STATUS "=== Compiler & Platform Detection ===")

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

# ============================================================================
# Compiler Flags
# ============================================================================
set(KONAMI_WARNING_FLAGS "")
set(KONAMI_RELEASE_FLAGS "")
set(KONAMI_DEBUG_FLAGS "")

if(KONAMI_COMPILER_GCC OR KONAMI_COMPILER_CLANG)
    list(APPEND KONAMI_WARNING_FLAGS
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        -Wno-missing-field-initializers
    )

    list(APPEND KONAMI_RELEASE_FLAGS
        -O3 -DNDEBUG
        -funroll-loops -ftree-vectorize
        -fomit-frame-pointer
    )

    if(KONAMI_NATIVE_OPTIMIZATIONS)
        list(APPEND KONAMI_RELEASE_FLAGS -march=native -mtune=native)
    endif()

    list(APPEND KONAMI_DEBUG_FLAGS
        -g3 -O0 -DDEBUG
        -fno-omit-frame-pointer
    )

    if(KONAMI_COMPILER_CLANG)
        list(APPEND KONAMI_WARNING_FLAGS
            -Wno-c++98-compat -Wno-c++98-compat-pedantic
            -Wno-padded -Wno-weak-vtables
        )
    endif()

    if(KONAMI_COMPILER_GCC)
        list(APPEND KONAMI_WARNING_FLAGS -Wlogical-op)
    endif()

elseif(KONAMI_COMPILER_MSVC OR KONAMI_COMPILER_CLANGCL)
    list(APPEND KONAMI_WARNING_FLAGS
        /W4 /permissive- /Zc:__cplusplus
        /wd4100 /wd4201 /wd4324 /wd4267 /wd4244
    )

    list(APPEND KONAMI_RELEASE_FLAGS
        /O2 /Ob3 /GL /Gy /Gw /DNDEBUG
        /fp:fast
    )

    list(APPEND KONAMI_DEBUG_FLAGS
        /Od /Zi /RTC1 /DDEBUG
    )
endif()

# LTO
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

# ============================================================================
# Sanitizers
# ============================================================================
set(KONAMI_SANITIZER_FLAGS "")

if(KONAMI_ENABLE_ASAN AND NOT KONAMI_COMPILER_MSVC)
    list(APPEND KONAMI_SANITIZER_FLAGS -fsanitize=address -fno-omit-frame-pointer)
endif()
if(KONAMI_ENABLE_TSAN AND NOT KONAMI_COMPILER_MSVC)
    list(APPEND KONAMI_SANITIZER_FLAGS -fsanitize=thread)
endif()
if(KONAMI_ENABLE_UBSAN AND NOT KONAMI_COMPILER_MSVC)
    list(APPEND KONAMI_SANITIZER_FLAGS -fsanitize=undefined)
endif()
if(KONAMI_ENABLE_MSAN AND KONAMI_COMPILER_CLANG)
    list(APPEND KONAMI_SANITIZER_FLAGS -fsanitize=memory -fno-omit-frame-pointer)
endif()

# ============================================================================
# Dependencies via FetchContent
# ============================================================================
message(STATUS "")
message(STATUS "=== Fetching Dependencies ===")

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# --- Slint UI Framework ---
message(STATUS "Fetching Slint UI Framework...")
FetchContent_Declare(
    Slint
    GIT_REPOSITORY https://github.com/slint-ui/slint.git
    GIT_TAG        v1.8.0
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

set(SLINT_FEATURE_BACKEND_WINIT ON CACHE BOOL "" FORCE)
set(SLINT_FEATURE_RENDERER_FEMTOVG ON CACHE BOOL "" FORCE)

if(KONAMI_ENABLE_VULKAN)
    set(SLINT_FEATURE_RENDERER_SKIA ON CACHE BOOL "" FORCE)
    set(SLINT_FEATURE_RENDERER_SKIA_VULKAN ON CACHE BOOL "" FORCE)
endif()

if(KONAMI_PLATFORM_WINDOWS AND KONAMI_ENABLE_DIRECTX)
    set(SLINT_FEATURE_RENDERER_SKIA ON CACHE BOOL "" FORCE)
    set(SLINT_FEATURE_RENDERER_SKIA_D3D ON CACHE BOOL "" FORCE)
endif()

if(KONAMI_PLATFORM_MACOS AND KONAMI_ENABLE_METAL)
    set(SLINT_FEATURE_RENDERER_SKIA ON CACHE BOOL "" FORCE)
    set(SLINT_FEATURE_RENDERER_SKIA_METAL ON CACHE BOOL "" FORCE)
endif()

FetchContent_MakeAvailable(Slint)

# --- nlohmann/json ---
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

# --- cpr (HTTP) ---
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

# --- spdlog ---
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

# --- Asio (header-only) ---
message(STATUS "Fetching Asio...")
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-30-2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(asio)

# Asio does not export a CMake target; create an interface library
add_library(asio_headers INTERFACE)
target_include_directories(asio_headers INTERFACE "${asio_SOURCE_DIR}/asio/include")
target_compile_definitions(asio_headers INTERFACE ASIO_STANDALONE)

# --- Compression: ZLIB ---
message(STATUS "Fetching compression libraries...")
FetchContent_Declare(
    zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG        v1.3.1
    GIT_SHALLOW    TRUE
)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zlib)

if(NOT TARGET ZLIB::ZLIB)
    if(TARGET zlib)
        add_library(ZLIB::ZLIB ALIAS zlib)
    elseif(TARGET zlibstatic)
        add_library(ZLIB::ZLIB ALIAS zlibstatic)
    endif()
endif()

# --- Compression: LZ4 ---
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

# --- Compression: ZSTD ---
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

# --- OpenSSL ---
find_package(OpenSSL REQUIRED)
message(STATUS "OpenSSL: ${OPENSSL_VERSION}")

# --- libzip ---
message(STATUS "Fetching libzip...")
FetchContent_Declare(
    libzip
    GIT_REPOSITORY https://github.com/nih-at/libzip.git
    GIT_TAG        v1.10.1
    GIT_SHALLOW    TRUE
)
set(BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(BUILD_REGRESS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_DOC OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(libzip)

# --- stb (header-only image library) ---
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(stb)

# stb has no CMake target; create an interface library
add_library(stb_headers INTERFACE)
target_include_directories(stb_headers INTERFACE "${stb_SOURCE_DIR}")

# --- stduuid ---
FetchContent_Declare(
    stduuid
    GIT_REPOSITORY https://github.com/mariusbancila/stduuid.git
    GIT_TAG        v1.2.3
    GIT_SHALLOW    TRUE
)
set(UUID_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(stduuid)

# --- date ---
FetchContent_Declare(
    date
    GIT_REPOSITORY https://github.com/HowardHinnant/date.git
    GIT_TAG        v3.0.1
    GIT_SHALLOW    TRUE
)
set(BUILD_TZ_LIB ON CACHE BOOL "" FORCE)
set(USE_SYSTEM_TZ_DB ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(date)

# --- magic_enum ---
FetchContent_Declare(
    magic_enum
    GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
    GIT_TAG        v0.9.6
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(magic_enum)

# --- CLI11 ---
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

# --- Tracy Profiler (optional) ---
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

# --- Testing: Catch2 ---
if(KONAMI_BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.7.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")
endif()

# --- Benchmarking: Google Benchmark (optional) ---
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

# ============================================================================
# Source Files -- matched to actual file tree
# ============================================================================
message(STATUS "")
message(STATUS "=== Configuring Source Files ===")

# Actual .cpp files that exist in the repository
set(KONAMI_SOURCES
    src/core/Application.cpp
    src/core/auth/AuthManager.cpp
    src/core/auth/Encryption.cpp
    src/core/auth/MicrosoftAuth.cpp
    src/core/auth/TokenStorage.cpp
    src/core/downloader/CacheManager.cpp
    src/core/downloader/DownloadManager.cpp
    src/core/downloader/MojangAPI.cpp
    src/core/launcher/GameLauncher.cpp
    src/core/mods/ModManager.cpp
    src/core/profile/ProfileManager.cpp
    src/core/skin/SkinEngine.cpp
    src/ui/bridge/UIBridge.cpp
    src/utils/FileUtils.cpp
    src/utils/HttpClient.cpp
    src/utils/JsonUtils.cpp
    src/utils/PlatformUtils.cpp
    src/utils/StringUtils.cpp
)

# ============================================================================
# Slint UI Files
# ============================================================================

# Main window entry point (Slint compiles from this root)
set(KONAMI_SLINT_MAIN
    src/ui/main-window.slint
)

# ============================================================================
# Main Executable
# ============================================================================
message(STATUS "")
message(STATUS "=== Building Main Executable ===")

add_executable(${PROJECT_NAME}
    src/main.cpp
    ${KONAMI_SOURCES}
)

# Windows: GUI application
if(KONAMI_PLATFORM_WINDOWS)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/windows/konami.rc")
        target_sources(${PROJECT_NAME} PRIVATE resources/windows/konami.rc)
    endif()
endif()

# macOS: App bundle
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

# Compile Slint UI sources
slint_target_sources(${PROJECT_NAME} ${KONAMI_SLINT_MAIN})

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
    $<$<CONFIG:RelWithDebInfo>:${KONAMI_RELEASE_FLAGS}>
)

if(KONAMI_SANITIZER_FLAGS)
    target_link_options(${PROJECT_NAME} PRIVATE ${KONAMI_SANITIZER_FLAGS})
endif()

# ============================================================================
# Link Libraries
# ============================================================================
target_link_libraries(${PROJECT_NAME} PRIVATE
    Slint::Slint
    nlohmann_json::nlohmann_json
    cpr::cpr
    spdlog::spdlog
    asio_headers
    ZLIB::ZLIB
    libzstd_static
    zip
    OpenSSL::SSL
    OpenSSL::Crypto
    stb_headers
    stduuid
    date::date
    date::date-tz
    magic_enum::magic_enum
    CLI11::CLI11
)

# LZ4: the FetchContent build may produce lz4_static or lz4
if(TARGET lz4_static)
    target_link_libraries(${PROJECT_NAME} PRIVATE lz4_static)
elseif(TARGET lz4)
    target_link_libraries(${PROJECT_NAME} PRIVATE lz4)
endif()

# Tracy
if(KONAMI_ENABLE_TRACY)
    target_link_libraries(${PROJECT_NAME} PRIVATE Tracy::TracyClient)
    target_compile_definitions(${PROJECT_NAME} PRIVATE KONAMI_ENABLE_TRACY)
endif()

# Platform libraries
if(KONAMI_PLATFORM_WINDOWS)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ws2_32 crypt32 bcrypt userenv shell32 advapi32
        ole32 uuid dwmapi uxtheme
    )
elseif(KONAMI_PLATFORM_MACOS)
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    find_library(SECURITY_LIBRARY Security REQUIRED)
    find_library(IOKIT_LIBRARY IOKit REQUIRED)
    find_library(FOUNDATION_LIBRARY Foundation REQUIRED)
    find_library(APPKIT_LIBRARY AppKit REQUIRED)
    find_library(COREFOUNDATION_LIBRARY CoreFoundation REQUIRED)

    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${COCOA_LIBRARY} ${SECURITY_LIBRARY} ${IOKIT_LIBRARY}
        ${FOUNDATION_LIBRARY} ${APPKIT_LIBRARY} ${COREFOUNDATION_LIBRARY}
        "-framework Metal" "-framework QuartzCore"
    )
elseif(KONAMI_PLATFORM_LINUX)
    find_package(Threads REQUIRED)
    find_package(PkgConfig QUIET)

    target_link_libraries(${PROJECT_NAME} PRIVATE
        Threads::Threads ${CMAKE_DL_LIBS}
    )

    if(PkgConfig_FOUND)
        pkg_check_modules(LIBSECRET libsecret-1)
        pkg_check_modules(DBUS dbus-1)

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
endif()

# ============================================================================
# Installation
# ============================================================================
include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources")
    install(DIRECTORY resources/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}
        OPTIONAL
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR} OPTIONAL)
endif()

if(KONAMI_PLATFORM_LINUX AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/linux/konami-client.desktop")
    install(FILES resources/linux/konami-client.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications OPTIONAL)
endif()

# ============================================================================
# CPack Packaging
# ============================================================================
set(CPACK_PACKAGE_NAME "KonamiClient")
set(CPACK_PACKAGE_VENDOR "${KONAMI_AUTHOR}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-${KONAMI_PLATFORM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

if(KONAMI_PLATFORM_WINDOWS)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME "Konami Client")
    set(CPACK_NSIS_PACKAGE_NAME "Konami Client")
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

# ============================================================================
# Build Summary
# ============================================================================
message(STATUS "")
message(STATUS "=== BUILD CONFIGURATION SUMMARY ===")
message(STATUS "  Project:      ${PROJECT_NAME} v${PROJECT_VERSION} (${KONAMI_CODENAME})")
message(STATUS "  Platform:     ${KONAMI_PLATFORM_NAME} (${CMAKE_SYSTEM_PROCESSOR})")
message(STATUS "  Compiler:     ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "  LTO:          ${KONAMI_ENABLE_LTO}")
message(STATUS "  Tests:        ${KONAMI_BUILD_TESTS}")
message(STATUS "  Tracy:        ${KONAMI_ENABLE_TRACY}")
message(STATUS "  Vulkan:       ${KONAMI_ENABLE_VULKAN}")
if(KONAMI_PLATFORM_WINDOWS)
    message(STATUS "  DirectX:      ${KONAMI_ENABLE_DIRECTX}")
endif()
if(KONAMI_PLATFORM_MACOS)
    message(STATUS "  Metal:        ${KONAMI_ENABLE_METAL}")
endif()
message(STATUS "  Output:       ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "===================================")
message(STATUS "")
