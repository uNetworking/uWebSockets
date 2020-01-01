/* This is a fuzz test of the permessage-deflate module */

#define WIN32_EXPORT

#include <cstdio>
#include <string>

/* We test the permessage deflate module */
#include "../src/PerMessageDeflate.h"

#include "helpers.h"

struct StaticData {
    uWS::ZlibContext zlibContext;

    uWS::InflationStream inflationStream;
    uWS::DeflationStream deflationStream;
} staticData;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    /* Why is this padded? */
    makeChunked(makePadded(data, size), size, [](const uint8_t *data, size_t size) {
        std::string_view inflation = staticData.inflationStream.inflate(&staticData.zlibContext, std::string_view((char *) data, size), 256);
        if (inflation.length() > 256) {
            /* Cause ASAN to freak out */
            delete (int *) (void *) 1;
        }
    });

    makeChunked(makePadded(data, size), size, [](const uint8_t *data, size_t size) {
        /* Always reset */
        staticData.deflationStream.deflate(&staticData.zlibContext, std::string_view((char *) data, size), true);
    });

    return 0;
}

