cmake_minimum_required(VERSION 3.4)

project(uWebSockets)


find_package(OpenSSL REQUIRED)
find_path(LIBUV_INCLUDE uv.h)
find_library(LIBUV_LIBRARY libuv)



set(PublicHeaders src/Asio.h
                  src/Backend.h
                  src/Epoll.h
                  src/Extensions.h
                  src/Group.h
                  src/HTTPSocket.h
                  src/Hub.h
                  src/Libuv.h
                  src/Networking.h
                  src/Node.h
                  src/Room.h
                  src/Socket.h
                  src/uWS.h
                  src/WebSocket.h
                  src/WebSocketProtocol.h)



set (SourceFiles src/Epoll.cpp
                 src/Extensions.cpp
                 src/Group.cpp
                 src/HTTPSocket.cpp
                 src/Hub.cpp
                 src/Networking.cpp
                 src/Node.cpp
                 src/Room.cpp
                 src/Socket.cpp
                 src/WebSocket.cpp)



add_library(uWS ${PublicHeaders} ${SourceFiles})
set_target_properties(uWS PROPERTIES PUBLIC_HEADER "${PublicHeaders}")

set_property(TARGET uWS PROPERTY CXX_STANDARD 11)
set_property(TARGET uWS PROPERTY CXX_STANDARD_REQUIRED ON)

target_include_directories(uWS PUBLIC ${LIBUV_INCLUDE} ${OPENSSL_INCLUDE_DIR})
if (BUILD_SHARED_LIBS)
    target_link_libraries(uWS PRIVATE OpenSSL::SSL OpenSSL::Crypto ${LIBUV_LIBRARY})
else()
    target_link_libraries(uWS PUBLIC OpenSSL::SSL OpenSSL::Crypto ${LIBUV_LIBRARY})
    target_compile_definitions(uWS PUBLIC UWEBSOCKET_STATIC)
endif()

if (WIN32)
    target_compile_definitions(uWS PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()





install(TARGETS uWS
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        PUBLIC_HEADER DESTINATION include/uWS)