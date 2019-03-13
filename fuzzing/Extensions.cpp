/* This is a fuzz test of the websocket extensions parser */

#define WIN32_EXPORT

#include <cstdio>
#include <string>

/* We test the websocket extensions parser */
#include "../src/WebSocketExtensions.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    {
        uWS::ExtensionsNegotiator<true> extensionsNegotiator(uWS::Options::PERMESSAGE_DEFLATE);
        extensionsNegotiator.readOffer({(char *) data, size});

        extensionsNegotiator.generateOffer();
        extensionsNegotiator.getNegotiatedOptions();
    }

    {
        uWS::ExtensionsNegotiator<true> extensionsNegotiator(uWS::Options::NO_OPTIONS);
        extensionsNegotiator.readOffer({(char *) data, size});

        extensionsNegotiator.generateOffer();
        extensionsNegotiator.getNegotiatedOptions();
    }

    {
        uWS::ExtensionsNegotiator<true> extensionsNegotiator(uWS::Options::CLIENT_NO_CONTEXT_TAKEOVER);
        extensionsNegotiator.readOffer({(char *) data, size});

        extensionsNegotiator.generateOffer();
        extensionsNegotiator.getNegotiatedOptions();
    }

    return 0;
}

