cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0054 OLD)
project(uWS C CXX)

# ------------------  enabled std+11 ------------------------------
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/dist/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/dist/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/dist/bin)
set(CMAKE_CC_FLAGS "${CMAKE_CC_FLAGS} -O3")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

file(GLOB sourcefiles src/*.cpp)
file(GLOB headerfiles src/*.h)
add_library(uWS SHARED ${sourcefiles})
file(COPY ${headerfiles} DESTINATION ${PROJECT_SOURCE_DIR}/dist/include/uWS)
include_directories(${PROJECT_SOURCE_DIR})
target_link_libraries(uWS ssl z)
