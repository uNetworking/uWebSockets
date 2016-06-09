cmake_minimum_required(VERSION 3.0)
set(CMAKE_BUILD_TYPE Release)
project (µWebSockets)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

add_subdirectory(examples)

add_library(uWS SHARED src/uWS.cpp)
target_include_directories(uWS PUBLIC src)
target_compile_features(uWS PRIVATE cxx_nullptr)
target_link_libraries (uWS LINK_PUBLIC uv)
target_link_libraries (uWS LINK_PUBLIC ssl)
target_link_libraries (uWS LINK_PUBLIC crypto)
target_link_libraries (uWS LINK_PUBLIC z)

if (UNIX)
target_link_libraries (uWS LINK_PUBLIC pthread)
endif (UNIX)


add_library(uWS_Client SHARED src/uWS_Client.cpp)
target_include_directories(uWS_Client PUBLIC src)
target_compile_features(uWS_Client PRIVATE cxx_nullptr)
target_link_libraries (uWS_Client LINK_PUBLIC uv)
target_link_libraries (uWS_Client LINK_PUBLIC ssl)
target_link_libraries (uWS_Client LINK_PUBLIC crypto)
target_link_libraries (uWS_Client LINK_PUBLIC z)

if (UNIX)
target_link_libraries (uWS_Client LINK_PUBLIC pthread)
endif (UNIX)
