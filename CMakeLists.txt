# CMake build configuration.

# Generate makefiles with:
# $ rm -rf install && rm -rf build && mkdir install && mkdir build && cd build && cmake -Wdev --warn-unitialized -D CMAKE_BUILD_TYPE=DEBUG -D CMAKE_CXX_COMPILER=g++ -D CMAKE_C_COMPILER=gcc -D CMAKE_INSTALL_PREFIX=../install -G "Unix Makefiles" -D CMAKE_VERBOSE_MAKEFILE=TRUE ..

cmake_minimum_required(VERSION 3.6)

project(uWS CXX C)

# Configure Toolchain

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Ofast")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -pedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pipe")

# OpenSSL

find_package(OpenSSL REQUIRED)

message("OpenSSL version: ${OPENSSL_VERSION}")
message("OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
message("OpenSSL libraries: ${OPENSSL_LIBRARIES}")

include_directories(${OPENSSL_INCLUDE_DIR})
link_libraries(${OPENSSL_LIBRARIES})

# Zlib

find_package(ZLIB REQUIRED)

message("ZLib version: ${ZLIB_VERSION_STRING}")
message("ZLib include dir: ${ZLIB_INCLUDE_DIRS}")
message("ZLib libraries: ${ZLIB_LIBRARIES}")

include_directories(${ZLIB_INCLUDE_DIRS})
link_libraries(${ZLIB_LIBRARIES})

# UV

option(WITH_LIBUV "with(out) libuv") # or set(WITH_LIBUV true/false)

if(WITH_LIBUV)

    # CMake gitlab currently has the FindLibUV module, but the module
    # is not widely distributed yet. We need to distribute a copy of
    # the module for convenience ... just for now to keep it standard.
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
                          "${CMAKE_SOURCE_DIR}/misc/cmake/Modules/")

    find_package(LibUV REQUIRED)

    message("LibUV version: ${LibUV_VERSION}")
    message("LibUV include dir: ${LibUV_INCLUDE_DIRS}")
    message("LibUV libraries: ${LibUV_LIBRARIES}")

    include_directories(${LibUV_INCLUDE_DIRS})
    link_libraries(${LibUV_LIBRARIES})
    add_definitions(-DUSE_LIBUV)

endif(WITH_LIBUV)

# Build Product Artifacts

add_subdirectory("src")
