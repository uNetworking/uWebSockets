cmake_minimum_required(VERSION 3.0)
project(uWS)

SET(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

# Default to shared libs if not set
if (NOT DEFINED BUILD_SHARED_LIBS)
    set(BUILD_SHARED_LIBS ON)
endif()
if (BUILD_SHARED_LIBS)
    message("-- building shared libs")
else()
    message("-- building static libs")
endif()

# REVISIT: maybe should default to Debug, and give installation
#          notes for building release?
# Default to release build if not set
if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(CMAKE_BUILD_TYPE Release)
endif()
message("-- build type: ${CMAKE_BUILD_TYPE}")

# Build uWebSockets Library
add_library(uWS
    src/Extensions.cpp
    src/Group.cpp
    src/Networking.cpp
    src/Hub.cpp
    src/Node.cpp
    src/WebSocket.cpp
    src/HTTPSocket.cpp
    src/Socket.cpp
    src/Epoll.cpp)
target_compile_options(uWS PUBLIC -std=c++11)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    message("-- stripping library")
    target_compile_options(uWS PUBLIC -s)
endif()

if (USE_LTO)
    message("-- using LTO")
    # CMake 3.9 supports "INTERPROCEDURAL_OPTIMIZATION" property, but set it
    # manually for now
    # set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    # set_target_properties(uWS PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
    target_compile_options(uWS PUBLIC -flto)
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        # TODO: clang doesn't support this flag?
        target_compile_options(uWS PUBLIC -fno-fat-lto-objects)
    endif()
endif()


# Build Test Binary

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(Z REQUIRED)
find_package(Libuv REQUIRED)

add_executable(testsBin tests/main.cpp)
target_include_directories(testsBin PRIVATE src/)
target_link_libraries(testsBin PRIVATE uWS)
target_link_libraries(testsBin PRIVATE ${OPENSSL_LIBRARIES})
target_link_libraries(testsBin PRIVATE ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(testsBin PRIVATE ${Z_LIBRARY})
target_link_libraries(testsBin PRIVATE ${LIBUV_LIBRARIES})

# "make install" support
# XXX: the old Makefile would try $PREFIX/lib64 first if present, do we want
#      to preserve that behavior?
install(TARGETS uWS DESTINATION lib)
install(DIRECTORY src/ DESTINATION include/uWS FILES_MATCHING PATTERN "*.h")
