# Set the CMake minimum version
cmake_minimum_required(VERSION 3.10)

# Project name
project(uWSLibEpollBenchmarker)
message(STATUS "Building ${PROJECT_NAME}")

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(../src ../uSockets/src)

# Linker flags for wrapped syscalls
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--wrap=recv,--wrap=bind,--wrap=listen,--wrap=send,--wrap=socket,--wrap=epoll_wait,--wrap=accept4,--wrap=epoll_ctl")

# Add the executable
add_executable(${PROJECT_NAME} ../examples/HelloWorld.cpp epoll_benchmarker.cpp)

# Link with uSockets library
target_link_libraries(${PROJECT_NAME} PRIVATE uSockets)
