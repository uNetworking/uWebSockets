/* This is a fuzz test of the websocket handshake generator */

#define WIN32_EXPORT

#include <cstdio>
#include <string>

/* We test the websocket handshake generator */
#include "../src/WebSocketHandshake.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    char output[28];
    if (size >= 24) {
        uWS::WebSocketHandshake::generate((char *) data, output);
    }

    return 0;
}

