# =============================================================================
# Custom FindZLIB module
# =============================================================================
# Intercepts find_package(ZLIB) calls from sub-projects (e.g. libzip) and
# redirects them to our FetchContent-built zlib instead of searching the
# system.  Falls back to the standard CMake FindZLIB if no FetchContent
# target is available.
# =============================================================================

if(TARGET ZLIB::ZLIB)
    # FetchContent zlib is already available as an aliased target.
    # Populate all the variables that FindZLIB would normally set so that
    # callers relying on either the target or the variables are satisfied.
    set(ZLIB_FOUND TRUE)
    set(ZLIB_VERSION_STRING "1.3.1")

    # Headers: zlib.h lives in the source tree, zconf.h is generated in
    # the binary tree.
    set(ZLIB_INCLUDE_DIR  "${zlib_SOURCE_DIR}" CACHE PATH   "zlib include directory (FetchContent)" FORCE)
    set(ZLIB_INCLUDE_DIRS "${zlib_SOURCE_DIR};${zlib_BINARY_DIR}")
    set(ZLIB_LIBRARY      ZLIB::ZLIB)
    set(ZLIB_LIBRARIES    ZLIB::ZLIB)

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(ZLIB
        REQUIRED_VARS ZLIB_LIBRARY ZLIB_INCLUDE_DIR
        VERSION_VAR   ZLIB_VERSION_STRING
    )
    return()
endif()

# No FetchContent zlib – fall back to the standard CMake module.
include(${CMAKE_ROOT}/Modules/FindZLIB.cmake)
