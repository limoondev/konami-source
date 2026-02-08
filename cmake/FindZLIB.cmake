# =============================================================================
# Custom FindZLIB module
# =============================================================================
# Intercepts find_package(ZLIB) calls from sub-projects (e.g. libzip) and
# redirects them to our FetchContent-built zlib instead of searching the
# system.  Falls back to the standard CMake FindZLIB if no FetchContent
# target is available.
#
# zlib v1.3.1 creates "zlibstatic" as an ALIAS of "zlib", so we cannot
# create another ALIAS (CMake forbids alias-of-alias) nor call
# target_include_directories on an alias.  We use an IMPORTED INTERFACE
# target to work around both restrictions.
# =============================================================================

# If ZLIB::ZLIB already exists (created by main CMakeLists), just set vars.
if(TARGET ZLIB::ZLIB)
    set(ZLIB_FOUND TRUE)
    set(ZLIB_VERSION_STRING "1.3.1")
    set(ZLIB_INCLUDE_DIR  "${zlib_SOURCE_DIR}" CACHE PATH   "" FORCE)
    set(ZLIB_INCLUDE_DIRS "${zlib_SOURCE_DIR};${zlib_BINARY_DIR}")
    set(ZLIB_LIBRARY      "${zlib_SOURCE_DIR}/zlib.h" CACHE FILEPATH "" FORCE)
    set(ZLIB_LIBRARIES    ZLIB::ZLIB)

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(ZLIB
        REQUIRED_VARS ZLIB_LIBRARY ZLIB_INCLUDE_DIR
        VERSION_VAR   ZLIB_VERSION_STRING
    )
    return()
endif()

# Raw FetchContent targets exist but ZLIB::ZLIB hasn't been created yet.
if(TARGET zlib OR TARGET zlibstatic)
    # Resolve the real (non-alias) target.
    set(_zlib_real "")
    foreach(_candidate zlib zlibstatic)
        if(TARGET ${_candidate})
            get_target_property(_alias ${_candidate} ALIASED_TARGET)
            if(_alias)
                set(_zlib_real ${_alias})
            else()
                set(_zlib_real ${_candidate})
            endif()
            break()
        endif()
    endforeach()

    if(_zlib_real)
        target_include_directories(${_zlib_real} PUBLIC
            "${zlib_SOURCE_DIR}"
            "${zlib_BINARY_DIR}"
        )

        add_library(ZLIB::ZLIB INTERFACE IMPORTED)
        set_target_properties(ZLIB::ZLIB PROPERTIES
            INTERFACE_LINK_LIBRARIES "${_zlib_real}"
            INTERFACE_INCLUDE_DIRECTORIES "${zlib_SOURCE_DIR};${zlib_BINARY_DIR}"
        )
    endif()

    set(ZLIB_FOUND TRUE)
    set(ZLIB_VERSION_STRING "1.3.1")
    set(ZLIB_INCLUDE_DIR  "${zlib_SOURCE_DIR}" CACHE PATH   "" FORCE)
    set(ZLIB_INCLUDE_DIRS "${zlib_SOURCE_DIR};${zlib_BINARY_DIR}")
    set(ZLIB_LIBRARY      "${zlib_SOURCE_DIR}/zlib.h" CACHE FILEPATH "" FORCE)
    set(ZLIB_LIBRARIES    ZLIB::ZLIB)

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(ZLIB
        REQUIRED_VARS ZLIB_LIBRARY ZLIB_INCLUDE_DIR
        VERSION_VAR   ZLIB_VERSION_STRING
    )
    return()
endif()

# No FetchContent zlib -- fall back to the standard CMake module.
include(${CMAKE_ROOT}/Modules/FindZLIB.cmake)
