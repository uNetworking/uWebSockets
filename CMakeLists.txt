cmake_minimum_required(VERSION 3.0)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_STANDARD 11)
project (µWebSockets)

option(BUILD_SHARED_LIBS "Build shared libraries." ON)
option(BUILD_STATIC_LIBS "Build static libraries." OFF)
option(BUILD_EXAMPLES "Build the examples" ON)

set(TARGET_NAME uWS)
set(SOURCES src/Extensions.cpp src/Group.cpp src/WebSocketImpl.cpp src/Networking.cpp src/Hub.cpp src/Node.cpp src/WebSocket.cpp src/HTTPSocket.cpp src/Socket.cpp)

if(NOT CMAKE_INSTALL_PREFIX)
    set(CMAKE_INSTALL_PREFIX /usr)
endif()

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

if(NOT LIBUV_INCLUDE_DIR)
    find_path(LIBUV_INCLUDE_DIR uv.h)
endif()
if(NOT LIBUV_LIBRARY)
    find_library(LIBUV_LIBRARY NAMES uv uv1)
endif()

if(CMAKE_VERSION VERSION_LESS "3.1")
    # CMake 3.0 does not implement "CMAKE_CXX_STANDARD":
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_CXX_FLAGS "--std=gnu++11 ${CMAKE_CXX_FLAGS}")
    endif()
endif()


if (BUILD_SHARED_LIBS)
    # Add the static library target
    add_library(${TARGET_NAME} ${SOURCES})

    # Include the source files
    target_include_directories(${TARGET_NAME} PUBLIC src)

    # Link against external libraries
    target_include_directories(${TARGET_NAME} PUBLIC ${LIBUV_INCLUDE_DIR})
    target_include_directories(${TARGET_NAME} PUBLIC ${ZLIB_INCLUDE_DIRS})
    target_include_directories(${TARGET_NAME} PUBLIC ${OPENSSL_INCLUDE_DIR})
    target_link_libraries (${TARGET_NAME} LINK_PUBLIC ${LIBUV_LIBRARY})
    target_link_libraries (${TARGET_NAME} LINK_PUBLIC ${OPENSSL_SSL_LIBRARY})
    target_link_libraries (${TARGET_NAME} LINK_PUBLIC ${OPENSSL_CRYPTO_LIBRARY})
    target_link_libraries (${TARGET_NAME} LINK_PUBLIC ${ZLIB_LIBRARY})

    if (UNIX)
        # Link against the pthread library
        target_link_libraries (${TARGET_NAME} LINK_PUBLIC pthread)
        # Install the resulting library into /usr/lib64 (by default)
        install (TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/lib64)
    endif(UNIX)
endif()


if (BUILD_STATIC_LIBS)
    # Add the static library target
    add_library(${TARGET_NAME}_static STATIC ${SOURCES})

    # Include the source files
    target_include_directories(${TARGET_NAME}_static PUBLIC src)

    # Link against external libraries
    target_include_directories(${TARGET_NAME}_static PUBLIC ${LIBUV_INCLUDE_DIR})
    target_include_directories(${TARGET_NAME}_static PUBLIC ${ZLIB_INCLUDE_DIRS})
    target_include_directories(${TARGET_NAME}_static PUBLIC ${OPENSSL_INCLUDE_DIR})
    target_link_libraries (${TARGET_NAME}_static LINK_PUBLIC ${LIBUV_LIBRARY})
    target_link_libraries (${TARGET_NAME}_static LINK_PUBLIC ${OPENSSL_SSL_LIBRARY})
    target_link_libraries (${TARGET_NAME}_static LINK_PUBLIC ${OPENSSL_CRYPTO_LIBRARY})
    target_link_libraries (${TARGET_NAME}_static LINK_PUBLIC ${ZLIB_LIBRARY})

    if (UNIX)
        # Link against the pthread library
        target_link_libraries (${TARGET_NAME}_static LINK_PUBLIC pthread)
        # Install the resulting library into /usr/lib64 (by default)
        install (TARGETS ${TARGET_NAME}_static DESTINATION ${CMAKE_INSTALL_PREFIX}/lib64)
    endif(UNIX)
endif()

if (UNIX)
    install (FILES src/Extensions.h src/WebSocketProtocol.h src/Networking.h src/WebSocket.h src/Hub.h src/Group.h src/Node.h src/Socket.h src/HTTPSocket.h src/uWS.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/uWS)
endif (UNIX)

if (BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
