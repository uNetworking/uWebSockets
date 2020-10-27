cmake_minimum_required(VERSION 3.11)

project(uwebsockets)

# -- Versioning information
set(${PROJECT_NAME}_normalized_version "18.15.0")

# -- CMake included modules
include(GNUInstallDirs)
include(CheckFunctionExists)
include(CMakePackageConfigHelpers)

# -- CMake build options
option(ENABLE_OPENSSL  "Enable OpenSSL support" OFF)
option(ENABLE_LIBUV    "Enable LibUV support"   OFF)
option(BUILD_EXAMPLES  "Build examples"         OFF)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/modules")

# -- Set c/cxx standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# -- Compression
find_package(ZLIB REQUIRED)

# -- SSL support?
if(ENABLE_OPENSSL)
  find_package(OpenSSL REQUIRED)
  add_compile_definitions(LIBUS_USE_OPENSSL)
else()
  add_compile_definitions(LIBUS_NO_SSL)
endif()
#add_compile_definitions(LIBUS_USE_WOLFSSL)

# -- LibUV support?
if(ENABLE_LIBUV)
  find_package(LibUV REQUIRED)
  add_compile_definitions(LIBUS_USE_LIBUV)
endif()

# -- Linux: Common location of cmake package files
set(ConfigPackageLocation ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

# -- Common sources
set(${PROJECT_NAME}_SOURCES_LIB
  ${CMAKE_CURRENT_LIST_DIR}/uSockets/src/socket.c
  ${CMAKE_CURRENT_LIST_DIR}/uSockets/src/loop.c
  ${CMAKE_CURRENT_LIST_DIR}/uSockets/src/context.c
  ${CMAKE_CURRENT_LIST_DIR}/uSockets/src/bsd.c
  ${CMAKE_CURRENT_LIST_DIR}/uSockets/src/eventing/gcd.c
  ${CMAKE_CURRENT_LIST_DIR}/uSockets/src/eventing/epoll_kqueue.c
)

# -- Optional sources
if(ENABLE_OPENSSL)
  list(APPEND ${PROJECT_NAME}_SOURCES_LIB
    "${CMAKE_CURRENT_LIST_DIR}/uSockets/src/crypto/openssl.c"
    "${CMAKE_CURRENT_LIST_DIR}/uSockets/src/crypto/sni_tree.cpp")
endif()

if(ENABLE_LIBUV)
  list(APPEND ${PROJECT_NAME}_SOURCES_LIB
    "${CMAKE_CURRENT_LIST_DIR}/uSockets/src/eventing/libuv.c")
endif()

# -- Detailed project setup
add_library(${PROJECT_NAME} SHARED)

target_sources(${PROJECT_NAME}
  PRIVATE
    ${${PROJECT_NAME}_SOURCES_LIB})

target_include_directories(${PROJECT_NAME}
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/uSockets/src
  PUBLIC
    $<INSTALL_INTERFACE:include>
    $<INSTALL_INTERFACE:include/uwebsockets>)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    ${OPENSSL_LIBRARIES}
    ${LIBUV_LIBRARIES})

set_target_properties(${PROJECT_NAME}
  PROPERTIES
    VERSION ${${PROJECT_NAME}_normalized_version}
    SOVERSION ${${PROJECT_NAME}_normalized_version})

install(TARGETS ${PROJECT_NAME}
  EXPORT ${PROJECT_NAME}_targets
  LIBRARY DESTINATION lib
  INCLUDES DESTINATION include;include/uwebsockets
)

install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/src/
  DESTINATION include/uwebsockets)

install(FILES ${CMAKE_CURRENT_LIST_DIR}/uSockets/src/libusockets.h
  DESTINATION include)

# -- Cmake config files
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}-config-version.cmake"
  VERSION ${${PROJECT_NAME}_normalized_version}
  COMPATIBILITY AnyNewerVersion)

configure_package_config_file(
  ${PROJECT_NAME}-config.cmake.in
  ${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config.cmake
  INSTALL_DESTINATION ${ConfigPackageLocation})

install(EXPORT ${PROJECT_NAME}_targets
  FILE ${PROJECT_NAME}-targets.cmake
  DESTINATION ${ConfigPackageLocation})

install(
  FILES
    ${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config.cmake
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}-config-version.cmake"
  DESTINATION ${ConfigPackageLocation}
  COMPONENT Development)

# -- Optional examples
if(BUILD_EXAMPLES)
  # -- Threading
  set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
  set(THREADS_PREFER_PTHREAD_FLAG TRUE)
  find_package(Threads REQUIRED)

  # -- Common definitions
  set(EXAMPLES_LIBS
    ${ZLIB_LIBRARIES} ${OPENSSL_LIBRARIES} ${LIBUV_LIBRARIES} uwebsockets)

  set(EXAMPLES_INCLUDES
    ${CMAKE_CURRENT_LIST_DIR}/uSockets/src ${CMAKE_CURRENT_LIST_DIR}/src)

  # -- Echo Server
  add_executable(uwebsockets-echoserver
    ${CMAKE_CURRENT_LIST_DIR}/examples/EchoServer.cpp)

  target_include_directories(uwebsockets-echoserver
    PRIVATE ${EXAMPLES_INCLUDES})

  target_link_libraries(uwebsockets-echoserver
    PRIVATE ${EXAMPLES_LIBS})

  install(TARGETS uwebsockets-echoserver
    RUNTIME DESTINATION bin)

  # -- Echo Server threaded
  add_executable(uwebsockets-echoserverthreaded
    ${CMAKE_CURRENT_LIST_DIR}/examples/EchoServerThreaded.cpp)

  target_include_directories(uwebsockets-echoserverthreaded
    PRIVATE ${EXAMPLES_INCLUDES})

  target_link_libraries(uwebsockets-echoserverthreaded
    PRIVATE ${EXAMPLES_LIBS} Threads::Threads)

  install(TARGETS uwebsockets-echoserverthreaded
    RUNTIME DESTINATION bin)

  # -- Broadcasting Echo Server
  add_executable(uwebsockets-broadcastingechoserver
    ${CMAKE_CURRENT_LIST_DIR}/examples/BroadcastingEchoServer.cpp)

  target_include_directories(uwebsockets-broadcastingechoserver
    PRIVATE ${EXAMPLES_INCLUDES})

  target_link_libraries(uwebsockets-broadcastingechoserver
    PRIVATE ${EXAMPLES_LIBS})

  install(TARGETS uwebsockets-broadcastingechoserver
    RUNTIME DESTINATION bin)

  # -- Hello World
  add_executable(uwebsockets-helloworld
    ${CMAKE_CURRENT_LIST_DIR}/examples/HelloWorld.cpp)

  target_include_directories(uwebsockets-helloworld
    PRIVATE ${EXAMPLES_INCLUDES})

  target_link_libraries(uwebsockets-helloworld
    PRIVATE ${EXAMPLES_LIBS})

  install(TARGETS uwebsockets-helloworld
    RUNTIME DESTINATION bin)

  # -- Hello World Threaded
  add_executable(uwebsockets-helloworldthreaded
    ${CMAKE_CURRENT_LIST_DIR}/examples/HelloWorldThreaded.cpp)

  target_include_directories(uwebsockets-helloworldthreaded
    PRIVATE ${EXAMPLES_INCLUDES})

  target_link_libraries(uwebsockets-helloworldthreaded
    PRIVATE ${EXAMPLES_LIBS} Threads::Threads)

  install(TARGETS uwebsockets-helloworldthreaded
    RUNTIME DESTINATION bin)

  if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 8.0)
    # -- HTTP Server (Needs compiler support for filesystem)
    add_executable(uwebsockets-httpserver
      ${CMAKE_CURRENT_LIST_DIR}/examples/HttpServer.cpp)

    target_include_directories(uwebsockets-httpserver
      PRIVATE ${EXAMPLES_INCLUDES})

    target_link_libraries(uwebsockets-httpserver
      PRIVATE ${EXAMPLES_LIBS} Threads::Threads)

    install(TARGETS uwebsockets-httpserver
      RUNTIME DESTINATION bin)
  endif()

  # -- Server Name
  add_executable(uwebsockets-servername
    ${CMAKE_CURRENT_LIST_DIR}/examples/ServerName.cpp)

  target_include_directories(uwebsockets-servername
    PRIVATE ${EXAMPLES_INCLUDES})

  target_link_libraries(uwebsockets-servername
    PRIVATE ${EXAMPLES_LIBS})

  install(TARGETS uwebsockets-servername
    RUNTIME DESTINATION bin)

  # -- Upgrade Async
  add_executable(uwebsockets-upgradeasync
    ${CMAKE_CURRENT_LIST_DIR}/examples/UpgradeAsync.cpp)

  target_include_directories(uwebsockets-upgradeasync
    PRIVATE ${EXAMPLES_INCLUDES})

  target_link_libraries(uwebsockets-upgradeasync
    PRIVATE ${EXAMPLES_LIBS})

  install(TARGETS uwebsockets-upgradeasync
    RUNTIME DESTINATION bin)

  # -- Upgrade Sync
  add_executable(uwebsockets-upgradesync
    ${CMAKE_CURRENT_LIST_DIR}/examples/UpgradeSync.cpp)

  target_include_directories(uwebsockets-upgradesync
    PRIVATE ${EXAMPLES_INCLUDES})

  target_link_libraries(uwebsockets-upgradesync
    PRIVATE ${EXAMPLES_LIBS})

  install(TARGETS uwebsockets-upgradesync
    RUNTIME DESTINATION bin)
endif()
