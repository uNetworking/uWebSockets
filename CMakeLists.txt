cmake_minimum_required(VERSION 2.8)
project(uWebSockets)

set (CMAKE_CXX_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

if(APPLE)
add_definitions(-stdlib=libc++ -mmacosx-version-min=10.7 -undefined dynamic_lookup)    
endif(APPLE)

##  PTHREAD
find_package(Threads REQUIRED)

## OpenSSL
find_package(PkgConfig REQUIRED)
pkg_search_module(OPENSSL REQUIRED openssl)

if( OPENSSL_FOUND )
    include_directories(${OPENSSL_INCLUDE_DIR})
    message(STATUS "Using OpenSSL ${OPENSSL_VERSION}")
else()
    # Error; with REQUIRED, pkg_search_module() will throw an error by it's own
    message(FATAL "OpeSSL no found")
endif()

## ZLIB
find_package(ZLIB REQUIRED)

if( ZLIB_FOUND )
    include_directories(${ZLIB_INCLUDE_DIRS})
    message(STATUS "Using Zlib ${ZLIB_VERSION_STRING}")
else()
    # Error; with REQUIRED, pkg_search_module() will throw an error by it's own
    message(FATAL "ZLib no found")
endif()


## Source files
set(TARGET uWS)
set(HEADER_DIR ${PROJECT_SOURCE_DIR}/src)
set(SRCS_DIR ${PROJECT_SOURCE_DIR}/src)

file(GLOB HEADERS ${HEADER_DIR}/*.h)
file(GLOB SRCS ${SRCS_DIR}/*.cpp)

add_library(${TARGET} SHARED ${SRCS} ${HEADERS})
set_target_properties(${TARGET} PROPERTIES INTERFACE_POSITION_INDEPENDENT_CODE ON
                                           PUBLIC_HEADER "${HEADERS}")
include(GNUInstallDirs)
install(TARGETS ${TARGET}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${TARGET})

## Building Tests
set(TEST_TARGET testsBin)
file(GLOB TEST_TARGET_SRSC ${PROJECT_SOURCE_DIR}/tests/*.cpp)
message(STATUS "Test Sources: ${HEADER_DIR}")
add_executable(${TEST_TARGET} ${TEST_TARGET_SRSC})
target_include_directories(${TEST_TARGET} PUBLIC ${HEADER_DIR})
target_link_libraries(${TEST_TARGET} ${TARGET} ${OPENSSL_LIBRARIES} ${ZLIB_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS})
