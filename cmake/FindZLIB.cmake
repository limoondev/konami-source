# =============================================================================
# Custom FindZLIB module
# =============================================================================
# Intercepts find_package(ZLIB) calls from sub-projects (e.g. libzip) and
# redirects them to our FetchContent-built zlib instead of searching the
# system.  Falls back to the standard CMake FindZLIB if no FetchContent
# target is available.
# =============================================================================

# Check for either the alias we create or the raw FetchContent targets.
if(TARGET ZLIB::ZLIB OR TARGET zlib OR TARGET zlibstatic)

    # Ensure the ZLIB::ZLIB alias exists (may not yet if this module
    # is invoked before the alias is created in the main CMakeLists).
    if(NOT TARGET ZLIB::ZLIB)
        if(TARGET zlibstatic)
            add_library(ZLIB::ZLIB ALIAS zlibstatic)
        elseif(TARGET zlib)
            add_library(ZLIB::ZLIB ALIAS zlib)
        endif()
    endif()

    # Resolve the real (non-alias) zlib target for target_include_directories
    # (CMake forbids calling target_include_directories on ALIAS targets).
    set(_zlib_real_target "")
    if(TARGET zlibstatic)
        set(_zlib_real_target zlibstatic)
    elseif(TARGET zlib)
        # "zlib" is either the real shared-lib target or an alias.
        get_target_property(_zlib_alias zlib ALIASED_TARGET)
        if(_zlib_alias)
            set(_zlib_real_target ${_zlib_alias})
        else()
            set(_zlib_real_target zlib)
        endif()
    endif()

    # Populate all the variables that FindZLIB would normally set so that
    # callers relying on either the target or the variables are satisfied.
    set(ZLIB_FOUND TRUE)
    set(ZLIB_VERSION_STRING "1.3.1")

    # zlib.h lives in the source tree, zconf.h is generated in the binary
    # tree. Provide both paths.
    set(ZLIB_INCLUDE_DIR  "${zlib_SOURCE_DIR}" CACHE PATH "zlib include directory (FetchContent)" FORCE)
    set(ZLIB_INCLUDE_DIRS "${zlib_SOURCE_DIR};${zlib_BINARY_DIR}")

    # ZLIB_LIBRARY needs a non-empty value for find_package_handle_standard_args.
    # The library isn't built yet at configure time, so use the source dir as a
    # sentinel. Consumers must link via the ZLIB::ZLIB target, not this variable.
    set(ZLIB_LIBRARY  "${zlib_SOURCE_DIR}/zlib.h" CACHE FILEPATH "zlib library (FetchContent)" FORCE)
    set(ZLIB_LIBRARIES ZLIB::ZLIB)

    # Propagate include directories on the real target so libzip (and anything
    # else linking ZLIB::ZLIB) can find zlib.h and zconf.h.
    if(_zlib_real_target)
        target_include_directories(${_zlib_real_target} PUBLIC
            "${zlib_SOURCE_DIR}"
            "${zlib_BINARY_DIR}"
        )
    endif()

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(ZLIB
        REQUIRED_VARS ZLIB_LIBRARY ZLIB_INCLUDE_DIR
        VERSION_VAR   ZLIB_VERSION_STRING
    )
    return()
endif()

# No FetchContent zlib -- fall back to the standard CMake module.
include(${CMAKE_ROOT}/Modules/FindZLIB.cmake)
