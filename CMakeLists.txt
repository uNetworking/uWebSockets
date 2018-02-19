cmake_minimum_required(VERSION 3.0)
set(REQUIRED_CPP_VERSION 11)

set(PROJECT_NAME uWS)
project(${PROJECT_NAME} CXX)

# gather the files
set(Backend_headers
  "${PROJECT_SOURCE_DIR}/src/Backend.h"
  "${PROJECT_SOURCE_DIR}/src/Asio.h"
  "${PROJECT_SOURCE_DIR}/src/Epoll.h"
  "${PROJECT_SOURCE_DIR}/src/Libuv.h"
)
set(uWS_headers
  "${PROJECT_SOURCE_DIR}/src/Extensions.h"
  "${PROJECT_SOURCE_DIR}/src/Group.h"
  "${PROJECT_SOURCE_DIR}/src/HTTPSocket.h"
  "${PROJECT_SOURCE_DIR}/src/Hub.h"
  "${PROJECT_SOURCE_DIR}/src/Networking.h"
  "${PROJECT_SOURCE_DIR}/src/Node.h"
  "${PROJECT_SOURCE_DIR}/src/Socket.h"
  "${PROJECT_SOURCE_DIR}/src/uWS.h"
  "${PROJECT_SOURCE_DIR}/src/WebSocket.h"
  "${PROJECT_SOURCE_DIR}/src/WebSocketProtocol.h"
)
set(Header_Files
  ${Backend_headers}
  ${uWS_headers}
)
FILE(GLOB Source_Files "${PROJECT_SOURCE_DIR}/src/*.cpp")

add_library(${PROJECT_NAME} STATIC ${Source_Files} ${Header_Files})

target_include_directories(${PROJECT_NAME} PRIVATE "src")

if (MSVC)
  # Project filters (to group files in IDE project)
  source_group("Header Files\\Backends" FILES ${Backend_headers})
  source_group("Source Files\\Backends" FILES "${PROJECT_SOURCE_DIR}/src/Epoll.cpp")
  set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD ${REQUIRED_CPP_VERSION})
  set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)
elseif(UNIX)
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++${REQUIRED_CPP_VERSION} -stdlib=libc++")
  target_link_libraries(${PROJECT_NAME} c++ c++abi pthread)
endif()
