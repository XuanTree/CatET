# Findraylib.cmake - Custom find module for raylib (MinGW builds without CMake config files)
#
# This module searches for raylib in common installation paths, including:
# - D:/RayLib_MinGW  (user's local MinGW raylib installation)
# - CMAKE_PREFIX_PATH
# - PATH environment variable
#
# Exports:
# raylib::raylib - imported target (IMPORTED_LOCATION + INTERFACE_INCLUDE_DIRECTORIES)

# Known raylib installation paths to search (in order of preference)
set(_raylib_search_paths
    "D:/RayLib_MinGW"
    "$ENV{RAYLIB_PATH}"
    "$ENV{HOME}/raylib"
    "$ENV{HOME}/RayLib_MinGW"
)

# Also append any paths from CMAKE_PREFIX_PATH
list(APPEND _raylib_search_paths ${CMAKE_PREFIX_PATH})

# Search for the include directory (raylib.h)
find_path(raylib_INCLUDE_DIR
    NAMES raylib.h
    PATHS ${_raylib_search_paths}
    PATH_SUFFIXES include
)

# Search for the static library
find_library(raylib_LIBRARY_STATIC
    NAMES libraylib.a raylib raylib_static
    PATHS ${_raylib_search_paths}
    PATH_SUFFIXES lib
)

# Search for the shared library import lib (for dll linking)
find_library(raylib_LIBRARY_DLL
    NAMES libraylibdll.a
    PATHS ${_raylib_search_paths}
    PATH_SUFFIXES lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(raylib
    REQUIRED_VARS raylib_INCLUDE_DIR raylib_LIBRARY_STATIC
)

if(raylib_FOUND AND NOT TARGET raylib::raylib)
    add_library(raylib::raylib STATIC IMPORTED)
    set_target_properties(raylib::raylib PROPERTIES
        IMPORTED_LOCATION "${raylib_LIBRARY_STATIC}"
        INTERFACE_INCLUDE_DIRECTORIES "${raylib_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "winmm;gdi32;opengl32"
    )
endif()

mark_as_advanced(raylib_INCLUDE_DIR raylib_LIBRARY_STATIC raylib_LIBRARY_DLL)
