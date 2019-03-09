/* This is a fuzz test of the http parser */

#define WIN32_EXPORT

/* We test the websocket parser */
#include "../src/HttpParser.h"

/* We use this to pad the fuzz */
char *padded = new char[1024  * 500];

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    /* Pad the fuzz */
    uWS::HttpParser httpParser;
    memcpy(padded, data, size);

    /* User data */
    void *user = (void *) 13;

    /* Parse it */
    httpParser.consumePostPadded(padded, size, user, [](void *s, uWS::HttpRequest *httpRequest) -> void * {

        /* todo: Route this via router */


        /* Return ok */
        return s;

    }, [](void *user, std::string_view data, bool fin) -> void * {

        /* Return ok */
        return user;

    }, [](void *user) {

        /* Return break */
        return nullptr;
    });

    return 0;
}

