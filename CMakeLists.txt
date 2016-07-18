cmake_minimum_required(VERSION 3.1)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_STANDARD 11)
project (µWebSockets)

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

find_path(LIBUV_INCLUDE_DIR uv.h)
find_library(LIBUV_LIBRARY NAMES uv uv1)

add_library(uWS SHARED src/Extensions.cpp src/HTTPSocket.cpp src/Network.cpp src/Server.cpp src/UTF8.cpp src/WebSocket.cpp)
target_include_directories(uWS PUBLIC src)

target_include_directories(uWS PUBLIC ${LIBUV_INCLUDE_DIR})
target_include_directories(uWS PUBLIC ${LIBUV_INCLUDE_DIR})
target_include_directories(uWS PUBLIC ${ZLIB_INCLUDE_DIRS})
target_include_directories(uWS PUBLIC ${OPENSSL_INCLUDE_DIR})
target_link_libraries (uWS LINK_PUBLIC ${LIBUV_LIBRARY})
target_link_libraries (uWS LINK_PUBLIC ${OPENSSL_SSL_LIBRARY})
target_link_libraries (uWS LINK_PUBLIC ${OPENSSL_CRYPTO_LIBRARY})
target_link_libraries (uWS LINK_PUBLIC ${ZLIB_LIBRARY})

if (UNIX)
target_link_libraries (uWS LINK_PUBLIC pthread)
endif (UNIX)

add_subdirectory(examples)
