cmake_minimum_required(VERSION 3.16)
project(uWebSocketsBenchmarks)
message(STATUS "Building ${PROJECT_NAME}")

# Set C and C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Enable LTO if supported
include(CheckIPOSupported)
check_ipo_supported(RESULT lto_supported OUTPUT error)
if(lto_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(WARNING "IPO / LTO is not supported: ${error}")
endif()

# Compile options: enable optimizations, march=native
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native")

# Add uSockets include directory
include_directories(${CMAKE_SOURCE_DIR}/libs/uWebSockets/uSockets/src)

# Define the parser executable
add_executable(${PROJECT_NAME}parser parser.cpp)
target_compile_options(${PROJECT_NAME}parser PRIVATE -flto)
target_link_options(${PROJECT_NAME}parser PRIVATE -flto)

# Compile tests with uSockets
add_executable(${PROJECT_NAME}broadcast_test broadcast_test.c)
target_compile_definitions(${PROJECT_NAME}broadcast_test PRIVATE -DLIBUS_USE_OPENSSL)
target_link_libraries(${PROJECT_NAME}broadcast_test ssl crypto ${LDFLAGS_STRIPPED_UWS} uSockets)
target_compile_options(${PROJECT_NAME}broadcast_test PRIVATE -flto)
target_link_options(${PROJECT_NAME}broadcast_test PRIVATE -flto)

add_executable(${PROJECT_NAME}load_test load_test.c)
target_compile_definitions(${PROJECT_NAME}load_test PRIVATE -DLIBUS_USE_OPENSSL)
target_link_libraries(${PROJECT_NAME}load_test ssl crypto ${LDFLAGS_STRIPPED_UWS} uSockets)
target_compile_options(${PROJECT_NAME}load_test PRIVATE -flto)
target_link_options(${PROJECT_NAME}load_test PRIVATE -flto)

add_executable(${PROJECT_NAME}scale_test scale_test.c)
target_compile_definitions(${PROJECT_NAME}scale_test PRIVATE -DLIBUS_USE_OPENSSL)
target_link_libraries(${PROJECT_NAME}scale_test ssl crypto ${LDFLAGS_STRIPPED_UWS} uSockets)
target_compile_options(${PROJECT_NAME}scale_test PRIVATE -flto)
target_link_options(${PROJECT_NAME}scale_test PRIVATE -flto)
