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

    # FetchContent zlib is already available as an aliased target.
    # Populate all the variables that FindZLIB would normally set so that
    # callers relying on either the target or the variables are satisfied.
    set(ZLIB_FOUND TRUE)
    set(ZLIB_VERSION_STRING "1.3.1")

    # zlib.h lives in the source tree, zconf.h is generated in the binary
    # tree. Point ZLIB_INCLUDE_DIR at the source tree (required by
    # find_package_handle_standard_args) and provide both in the DIRS list.
    set(ZLIB_INCLUDE_DIR  "${zlib_SOURCE_DIR}" CACHE PATH "zlib include directory (FetchContent)" FORCE)
    set(ZLIB_INCLUDE_DIRS "${zlib_SOURCE_DIR};${zlib_BINARY_DIR}")

    # ZLIB_LIBRARY must be a file path for FindPackageHandleStandardArgs to
    # accept it. Provide a well-known sentinel that passes the REQUIRED_VARS
    # check, then make sure all consumers actually link via the IMPORTED target.
    get_target_property(_zlib_loc ZLIB::ZLIB ALIASED_TARGET)
    if(_zlib_loc)
        # ALIAS -- resolve to real target's location if available
        get_target_property(_zlib_file ${_zlib_loc} LOCATION)
    else()
        get_target_property(_zlib_file ZLIB::ZLIB LOCATION)
    endif()
    if(NOT _zlib_file OR _zlib_file MATCHES "NOTFOUND")
        # During configure the library may not be built yet.
        # Use the include dir as a stand-in so the REQUIRED_VARS check passes.
        set(_zlib_file "${ZLIB_INCLUDE_DIR}")
    endif()
    set(ZLIB_LIBRARY  "${_zlib_file}" CACHE FILEPATH "zlib library (FetchContent)" FORCE)
    set(ZLIB_LIBRARIES ZLIB::ZLIB)

    # Make sure libzip (and anything else) can find the headers via the
    # target's interface include directories as well.
    foreach(_dir "${zlib_SOURCE_DIR}" "${zlib_BINARY_DIR}")
        if(TARGET zlib)
            target_include_directories(zlib PUBLIC "${_dir}")
        endif()
        if(TARGET zlibstatic)
            target_include_directories(zlibstatic PUBLIC "${_dir}")
        endif()
    endforeach()

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(ZLIB
        REQUIRED_VARS ZLIB_LIBRARY ZLIB_INCLUDE_DIR
        VERSION_VAR   ZLIB_VERSION_STRING
    )
    return()
endif()

# No FetchContent zlib -- fall back to the standard CMake module.
include(${CMAKE_ROOT}/Modules/FindZLIB.cmake)
