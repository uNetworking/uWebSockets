cmake_minimum_required(VERSION 3.10)

project(uWebSockets)
message(STATUS "Building ${PROJECT_NAME}")

# Compiler flags
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Set some default CXXFLAGS and LDFLAGS if not already set
if (NOT DEFINED CXXFLAGS)
    set(CXXFLAGS "-march=native -O3 -Wpedantic -Wall -Wextra -Wsign-conversion -Wconversion")
endif()

set(LDFLAGS_UWS "")

# If not building with LTO, remove the flag
option(WITH_LTO "Enable Link-Time Optimization" ON)
if (WITH_LTO)
    set(CXXFLAGS "${CXXFLAGS} -flto")
endif()

# Optionally build with Zlib
option(WITH_ZLIB "Enable Zlib (permessage-deflate)" ON)
if (WITH_ZLIB)
    find_package(ZLIB REQUIRED)
    set(LDFLAGS_UWS "${LDFLAGS_UWS} ${ZLIB_LIBRARIES}")
else()
    add_definitions(-DUWS_NO_ZLIB)
endif()

# Optionally build with BoringSSL
option(WITH_BORINGSSL "Build with BoringSSL support" OFF)
option(WITH_OPENSSL "Build with OpenSSL support" OFF)
option(WITH_WOLFSSL "Build with WolfSSL support" OFF)

if (WITH_BORINGSSL)
    include_directories(uSockets/boringssl/include)
    set(LDFLAGS_UWS "${LDFLAGS_UWS} uSockets/boringssl/build/ssl/libssl.a uSockets/boringssl/build/crypto/libcrypto.a")
    add_definitions(-DLIBUS_USE_OPENSSL)
elseif (WITH_OPENSSL)
    find_package(OpenSSL REQUIRED)
    set(LDFLAGS_UWS "${LDFLAGS_UWS} ${OPENSSL_LIBRARIES}")
elseif (WITH_WOLFSSL)
    set(LDFLAGS_UWS "${LDFLAGS_UWS} -L/usr/local/lib -lwolfssl")
endif()

# Optionally build with QUIC (Http3)
option(WITH_QUIC "Build with Http3 QUIC support" OFF)
if (WITH_QUIC)
    add_definitions(-DLIBUS_USE_QUIC)
    set(LDFLAGS_UWS "${LDFLAGS_UWS} -pthread -lz -lm uSockets/lsquic/src/liblsquic/liblsquic.a")
endif()

# Optionally build with libuv or ASIO as event loop
option(WITH_LIBUV "Build with libuv event loop" OFF)
option(WITH_ASIO "Build with ASIO event loop" OFF)

if (WITH_LIBUV)
    find_package(libuv CONFIG REQUIRED)
    set(LDFLAGS_UWS "${LDFLAGS_UWS} ${LIBUV_LIBRARIES}")
elseif (WITH_ASIO)
    set(CXXFLAGS "${CXXFLAGS} -pthread")
    set(LDFLAGS_UWS "${LDFLAGS_UWS} -lpthread")
endif()

# Optionally build with AddressSanitizer (ASAN)
option(WITH_ASAN "Build with AddressSanitizer" OFF)
if (WITH_ASAN)
    set(CXXFLAGS "${CXXFLAGS} -fsanitize=address -g")
    set(LDFLAGS_UWS "${LDFLAGS_UWS} -lasan")
endif()

# Include paths
include_directories(src)
include_directories(uSockets/src)

# uSockets target (as it was in your Makefile)
add_subdirectory(uSockets)

# Add example executables
set(EXAMPLE_FILES "CachingApp" "HelloWorldThreaded" "Http3Server" "Broadcast" "HelloWorld" "Crc32" "ServerName"
        "EchoServer" "BroadcastingEchoServer" "UpgradeSync" "UpgradeAsync" "ParameterRoutes")


string(STRIP "${LDFLAGS_UWS}" LDFLAGS_STRIPPED_UWS)  # Strip leading/trailing whitespace

foreach(EXAMPLE_FILE IN LISTS EXAMPLE_FILES)
    set(target_name ${EXAMPLE_FILE})
    add_executable(${target_name} examples/${EXAMPLE_FILE}.cpp)
    target_link_libraries(${target_name} uSockets ${LDFLAGS_STRIPPED_UWS})
    target_compile_options(${target_name} PRIVATE ${CXXFLAGS})
endforeach()


# Install target (mimicking the 'install' section of your Makefile)
install(DIRECTORY src/ DESTINATION ${CMAKE_INSTALL_PREFIX}/include/uWebSockets)

# Clean target (CMake provides a clean target natively)
# If additional custom clean behavior is required, you can add commands here.

# Default target is all
add_custom_target(all_examples DEPENDS ${EXAMPLE_FILES})

add_subdirectory(tests)
add_subdirectory(benchmarks)
# If linux, add epoll benchmarker
if (UNIX AND NOT APPLE)
    add_subdirectory(libEpollBenchmarker)
endif ()

