cmake_minimum_required(VERSION 2.6)

# This CMakeLists is to build the uSockets submodule.
# uWebSockets itself is header-only.

project(u-sockets)
file(GLOB_RECURSE sources "${CMAKE_CURRENT_LIST_DIR}/uSockets/src/*.c")
file(GLOB_RECURSE headers "${CMAKE_CURRENT_LIST_DIR}/uSockets/src/*.h")
add_library(u-sockets ${sources} ${headers})
target_include_directories(u-sockets PRIVATE "${CMAKE_CURRENT_LIST_DIR}/uSockets/src" "${CMAKE_CURRENT_LIST_DIR}/libuv")
target_compile_definitions(u-sockets PUBLIC LIBUS_NO_SSL UWS_NO_ZLIB)
