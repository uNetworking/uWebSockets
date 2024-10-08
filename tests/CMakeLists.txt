cmake_minimum_required(VERSION 3.10)

# Project name and language
project(uWebSocketsTests)
message(STATUS "Building ${PROJECT_NAME}")

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Address sanitizer flag
set(SANITIZE_FLAGS "-fsanitize=address")

# Define targets names with ${PROJECT_NAME} prefix
set(Query ${PROJECT_NAME}Query)
set(ChunkedEncoding ${PROJECT_NAME}ChunkedEncoding)
set(TopicTree ${PROJECT_NAME}TopicTree)
set(HttpRouter ${PROJECT_NAME}HttpRouter)
set(BloomFilter ${PROJECT_NAME}BloomFilter)
set(ExtensionsNegotiator ${PROJECT_NAME}ExtensionsNegotiator)
set(HttpParser ${PROJECT_NAME}HttpParser)

# Define the executables and their sources
add_executable(${Query} Query.cpp)
add_executable(${ChunkedEncoding} ChunkedEncoding.cpp)
add_executable(${TopicTree} TopicTree.cpp)
add_executable(${HttpRouter} HttpRouter.cpp)
add_executable(${BloomFilter} BloomFilter.cpp)
add_executable(${ExtensionsNegotiator} ExtensionsNegotiator.cpp)
add_executable(${HttpParser} HttpParser.cpp)

# Add the address sanitizer flags to each target
target_compile_options(${Query} PRIVATE ${SANITIZE_FLAGS})
target_link_options(${Query} PRIVATE ${SANITIZE_FLAGS})

target_compile_options(${ChunkedEncoding} PRIVATE ${SANITIZE_FLAGS})
target_link_options(${ChunkedEncoding} PRIVATE ${SANITIZE_FLAGS})

target_compile_options(${TopicTree} PRIVATE ${SANITIZE_FLAGS})
target_link_options(${TopicTree} PRIVATE ${SANITIZE_FLAGS})

target_compile_options(${HttpRouter} PRIVATE ${SANITIZE_FLAGS})
target_link_options(${HttpRouter} PRIVATE ${SANITIZE_FLAGS})

target_compile_options(${BloomFilter} PRIVATE ${SANITIZE_FLAGS})
target_link_options(${BloomFilter} PRIVATE ${SANITIZE_FLAGS})

target_compile_options(${ExtensionsNegotiator} PRIVATE ${SANITIZE_FLAGS})
target_link_options(${ExtensionsNegotiator} PRIVATE ${SANITIZE_FLAGS})

target_compile_options(${HttpParser} PRIVATE ${SANITIZE_FLAGS})
target_link_options(${HttpParser} PRIVATE ${SANITIZE_FLAGS})

# Performance target
add_custom_target(${PROJECT_NAME}performance
        COMMAND ${CMAKE_COMMAND} -E env CXXFLAGS=-O3 ${CMAKE_COMMAND} --build . --target HttpRouter
        COMMAND ./HttpRouter
)

# Smoke test target
add_custom_target(${PROJECT_NAME}smoke
        COMMAND ../Crc32 &
        COMMAND ${CMAKE_COMMAND} -E sleep 1
        COMMAND ~/.deno/bin/deno run --allow-net smoke.mjs
        COMMAND node smoke.mjs
        COMMAND pkill Crc32
)
