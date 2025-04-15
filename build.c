#include "build.h"

int main(int argc, char **argv) {
    /* Some variables we need */
    char *CXXFLAGS = strncpy(calloc(1024, 1), maybe(getenv("CXXFLAGS")), 1024);
    char *CFLAGS = strncpy(calloc(1024, 1), maybe(getenv("CFLAGS")), 1024);
    char *LDFLAGS = strncpy(calloc(1024, 1), maybe(getenv("LDFLAGS")), 1024);
    char *CC = strncpy(calloc(1024, 1), or_else(getenv("CC"), "cc"), 1024);
    char *CXX = strncpy(calloc(1024, 1), or_else(getenv("CXX"), "g++"), 1024);
    char *EXEC_SUFFIX = strncpy(calloc(1024, 1), maybe(getenv("EXEC_SUFFIX")), 1024);

    char *EXAMPLE_FILES[] = {"SecureGzipFileServer", "Precompress", "EchoBody", "HelloWorldThreaded", "Http3Server", "Broadcast", "HelloWorld", "Crc32", "ServerName",
    "EchoServer", "BroadcastingEchoServer", "UpgradeSync", "UpgradeAsync", "ParameterRoutes"};

    strcat(CXXFLAGS, " -march=native -O3 -Wpedantic -Wall -Wextra -Wsign-conversion -Wconversion -std=c++2b -Isrc -IuSockets/src");
    strcat(LDFLAGS, " uSockets/*.o");

    // We can use libdeflate as a fast path to zlib (you need to build it first)
    if (env_is("WITH_LIBDEFLATE", "1")) {
        strcat(LDFLAGS, " libdeflate/libdeflate.a");
        strcat(CXXFLAGS, " -DUWS_USE_LIBDEFLATE -I libdeflate");
    }

    // By default we use LTO, but Windows does not support it
    if (!env_is("WITH_LTO", "0")) {
        strcat(CXXFLAGS, " -flto=auto");
    }

    // By default we use zlib but you can build without it (disables permessage-deflate)
    if (!env_is("WITH_ZLIB", "0")) {
        strcat(LDFLAGS, " -lz");
    } else {
        strcat(CXXFLAGS, " -DUWS_NO_ZLIB");
    }

    // WITH_PROXY enables PROXY Protocol v2 support
    if (env_is("WITH_PROXY", "1")) {
        strcat(CXXFLAGS, " -DUWS_WITH_PROXY");
    }

    // WITH_QUIC enables experimental Http3 examples
    if (env_is("WITH_QUIC", "1")) {
        strcat(CXXFLAGS, " -DLIBUS_USE_QUIC");
        strcat(LDFLAGS, " -pthread -lz -lm uSockets/lsquic/src/liblsquic/liblsquic.a");
    }

    // Heavily prefer boringssl over openssl
    if (env_is("WITH_BORINGSSL", "1")) {
        strcat(CFLAGS, " -I uSockets/boringssl/include -pthread -DLIBUS_USE_OPENSSL");
        strcat(LDFLAGS, " -pthread uSockets/boringssl/build/ssl/libssl.a uSockets/boringssl/build/crypto/libcrypto.a");
    } else {
        // WITH_OPENSSL=1 enables OpenSSL 1.1+ support
        if (env_is("WITH_OPENSSL", "1")) {
            // With problems on macOS, make sure to pass needed LDFLAGS required to find these
            strcat(LDFLAGS, " -lssl -lcrypto");
        } else {
            // WITH_WOLFSSL=1 enables WolfSSL 4.2.0 support (mutually exclusive with OpenSSL)
            if (env_is("WITH_WOLFSSL", "1")) {
                strcat(LDFLAGS, " -L/usr/local/lib -lwolfssl");
            }
        }
    }

    // WITH_LIBUV=1 builds with libuv as event-loop
    if (env_is("WITH_LIBUV", "1")) {
        strcat(LDFLAGS, " -luv");
    }

    // WITH_ASIO=1 builds with ASIO as event-loop
    if (env_is("WITH_ASIO", "1")) {
        strcat(CXXFLAGS, " -pthread");
        strcat(LDFLAGS, " -lpthread");
    }

    // WITH_ASAN builds with sanitizers
    if (env_is("WITH_ASAN", "1")) {
        strcat(CXXFLAGS, " -fsanitize=address -g");
        strcat(LDFLAGS, " -lasan");
    }

    if (!strcmp(argv[1], "examples")) {
        #pragma omp parallel for
        for (int i = 0; i < sizeof(EXAMPLE_FILES) / sizeof(char *); i++) {
            if (run("%s %s examples/%s.cpp %s -o %s%s", CXX, CXXFLAGS, EXAMPLE_FILES[i], LDFLAGS, EXAMPLE_FILES[i], EXEC_SUFFIX)) {
                exit(-1);
            }
        }
    } else if (!strcmp(argv[1], "capi")) {
        printf("capi target does nothing yet\n");
    } else if (!strcmp(argv[1], "clean")) {
        printf("clean target does nothing yet\n");
    } else if (!strcmp(argv[1], "install")) {
        // install target is not even supposed to be cross platform
        printf("install target does nothing yet\n");
    } else if (!strcmp(argv[1], "all")) {
        printf("all target does nothing yet\n");
    }
}
