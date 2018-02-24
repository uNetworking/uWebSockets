# Try to find zlib
# Once done, this will define
#
# Z_FOUND          - system has zlib
# Z_INCLUDE_DIRS   - zlib include directories
# Z_LIBRARY        - zlib library
# Z_LIBRARY_STATIC - static zlib library

include(FindPackageHandleStandardArgs)

if(Z_INCLUDE_DIRS AND Z_LIBRARY AND Z_LIBRARY_STATIC)
    set(Z_FIND_QUIETLY TRUE)
else()
    find_path(
        Z_INCLUDE_DIR
        NAMES zlib.h
        HINTS ${Z_ROOT_DIR}
        PATH_SUFFIXES include)

    find_library(
        Z_LIBRARY
        NAMES z
        HINTS ${Z_ROOT_DIR}
        PATH_SUFFIXES ${LIBRARY_PATH_PREFIX})

    set(Z_INCLUDE_DIRS ${Z_INCLUDE_DIR})

    if( MacOSX )
        find_package_handle_standard_args(
            z
            DEFAULT_MSG
            Z_LIBRARY Z_INCLUDE_DIR)

        mark_as_advanced(Z_LIBRARY Z_INCLUDE_DIR)
    else ()
        find_library(
            Z_LIBRARY_STATIC
            NAMES libz.a
            HINTS ${Z_ROOT_DIR}
            PATH_SUFFIXES ${LIBRARY_PATH_PREFIX})

        find_package_handle_standard_args(
            z
            DEFAULT_MSG
            Z_LIBRARY Z_INCLUDE_DIR Z_LIBRARY_STATIC)

        mark_as_advanced(Z_LIBRARY Z_LIBRARY_STATIC Z_INCLUDE_DIR)
    endif()
endif()
