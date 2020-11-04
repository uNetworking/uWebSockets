cmake_minimum_required(VERSION 2.6)

# This CMakeLists is to build the uSockets submodule.
# uWebSockets itself is header-only.

project(usockets)
file(GLOB_RECURSE sources "uSockets/src/*.c")
file(GLOB_RECURSE headers "uSockets/src/*.h")
add_library(usockets ${sources} ${headers})
target_include_directories(usockets PRIVATE "uSockets/src" "libuv")
target_compile_definitions(usockets PUBLIC LIBUS_NO_SSL)
