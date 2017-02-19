project (µWS)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

if(NOT LIBUV_INCLUDE_DIR)
    find_path(LIBUV_INCLUDE_DIR uv.h)
endif()
if(NOT LIBUV_LIBRARY)
    find_library(LIBUV_LIBRARY NAMES uv uv1)
endif()

if(WIN32)
	if(BUILD_SHARED_LIBS)
		add_definitions(-DUWS_SHARED)
	endif()
endif()

add_library(uWS Extensions.cpp Group.cpp WebSocketImpl.cpp Networking.cpp Hub.cpp Node.cpp WebSocket.cpp HTTPSocket.cpp Socket.cpp)
target_include_directories(uWS PUBLIC src)

target_include_directories(uWS PUBLIC ${LIBUV_INCLUDE_DIR})
target_include_directories(uWS PUBLIC ${ZLIB_INCLUDE_DIRS})
target_include_directories(uWS PUBLIC ${OPENSSL_INCLUDE_DIR})
target_link_libraries (uWS LINK_PUBLIC ${LIBUV_LIBRARY})
target_link_libraries (uWS LINK_PUBLIC ${OPENSSL_SSL_LIBRARY})
target_link_libraries (uWS LINK_PUBLIC ${OPENSSL_CRYPTO_LIBRARY})
target_link_libraries (uWS LINK_PUBLIC ${ZLIB_LIBRARY})

if (UNIX)
target_link_libraries (uWS LINK_PUBLIC pthread)
install (TARGETS uWS DESTINATION /usr/lib64)
install (FILES Extensions.h WebSocketProtocol.h Networking.h WebSocket.h Hub.h Group.h Node.h Socket.h HTTPSocket.h uWS.h DESTINATION /usr/include/uWS)
endif (UNIX)

