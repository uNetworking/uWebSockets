/* This is a fuzz test of the http parser */

#define WIN32_EXPORT

/* We test the websocket parser */
#include "../src/HttpParser.h"

/* And the router */
#include "../src/HttpRouter.h"

/* We use this to pad the fuzz */
char *padded = new char[1024  * 500];

struct StaticData {

    struct RouterData {

    };

    uWS::HttpRouter<RouterData> router;

    StaticData() {

        router.add("get", "/:hello/:hi", [](RouterData &user, std::pair<int, std::string_view *> params) mutable {

            /* This route did handle it */
            return true;
        });

        router.add("post", "/:hello/:hi/*", [](RouterData &user, std::pair<int, std::string_view *> params) mutable {

            /* This route did handle it */
            return true;
        });

        router.add("get", "/*", [](RouterData &user, std::pair<int, std::string_view *> params) mutable {

            /* This route did not handle it */
            return false;
        });

        router.add("get", "/hi", [](RouterData &user, std::pair<int, std::string_view *> params) mutable {

            /* This route did handle it */
            return true;
        });
    }
} staticData;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    /* Pad the fuzz */
    uWS::HttpParser httpParser;
    memcpy(padded, data, size);

    /* User data */
    void *user = (void *) 13;

    /* Parse it */
    httpParser.consumePostPadded(padded, size, user, [](void *s, uWS::HttpRequest *httpRequest) -> void * {

        /* todo: Route this via router */

        httpRequest->getHeader(httpRequest->getUrl());
        httpRequest->getMethod();
        httpRequest->getQuery();

        /* Route the method and URL in two passes */
        StaticData::RouterData routerData = {};
        if (!staticData.router.route(httpRequest->getMethod(), httpRequest->getUrl(), routerData)) {
            /* It was not handled */
            return nullptr;
        }

        for (auto p : *httpRequest) {

        }

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

