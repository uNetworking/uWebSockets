cmake_minimum_required(VERSION 3.0)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_STANDARD 11)
project (µWebSockets)

option(BUILD_SHARED_LIBS "Build shared libraries." ON)

if(CMAKE_VERSION VERSION_LESS "3.1")
    # CMake 3.0 does not implement "CMAKE_CXX_STANDARD":
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_CXX_FLAGS "--std=gnu++11 ${CMAKE_CXX_FLAGS}")
    endif()
endif()

add_subdirectory(src)
add_subdirectory(examples)
