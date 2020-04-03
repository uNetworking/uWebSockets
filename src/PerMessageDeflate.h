/*
 * Authored by Alex Hultman, 2018-2019.
 * Intellectual property of third-party.

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 *     http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* This standalone module implements deflate / inflate streams */

#ifndef UWS_PERMESSAGEDEFLATE_H
#define UWS_PERMESSAGEDEFLATE_H

/* We always define these options no matter if ZLIB is enabled or not */
namespace uWS {
    /* Compress options (really more like PerMessageDeflateOptions) */
    enum CompressOptions : int {
        /* Compression disabled */
        DISABLED = 0,
        /* We compress using a shared non-sliding window. No added memory usage, worse compression. */
        SHARED_COMPRESSOR = 1,
        /* We compress using a dedicated sliding window. Major memory usage added, better compression of similarly repeated messages. */
        DEDICATED_COMPRESSOR = 2,
        /* Flags for limiting memory usage of dedicated compressor */
        DEDICATED_COMPRESSOR_3KB = 2 | 4,
        DEDICATED_COMPRESSOR_4KB = 2 | 8,
        DEDICATED_COMPRESSOR_8KB = 2 | 16,
        DEDICATED_COMPRESSOR_16KB = 2 | 32,
        DEDICATED_COMPRESSOR_32KB = 2 | 64,
        DEDICATED_COMPRESSOR_64KB = 2 | 128,
        DEDICATED_COMPRESSOR_128KB = 2 | 256,
        DEDICATED_COMPRESSOR_256KB = 2 | 512
    };
}

#ifndef UWS_NO_ZLIB
#include <zlib.h>
#endif

#include <string>

namespace uWS {

/* Do not compile this module if we don't want it */
#ifdef UWS_NO_ZLIB
struct ZlibContext {};
struct InflationStream {
    std::pair<std::string_view, bool> inflate(ZlibContext *zlibContext, std::string_view compressed, size_t maxPayloadLength) {
        /* Anything here goes, it is never going to be called */
        return {compressed, false};
    }
};
struct DeflationStream {
    std::string_view deflate(ZlibContext *zlibContext, std::string_view raw, bool reset) {
        return raw;
    }
    DeflationStream(int compressOptions) {
    }
};
#else

#define LARGE_BUFFER_SIZE 1024 * 16 // todo: fix this

struct ZlibContext {
    /* Any returned data is valid until next same-class call.
     * We need to have two classes to allow inflation followed
     * by many deflations without modifying the inflation */
    std::string dynamicDeflationBuffer;
    std::string dynamicInflationBuffer;
    char *deflationBuffer;
    char *inflationBuffer;

    ZlibContext() {
        deflationBuffer = (char *) malloc(LARGE_BUFFER_SIZE);
        inflationBuffer = (char *) malloc(LARGE_BUFFER_SIZE);
    }

    ~ZlibContext() {
        free(deflationBuffer);
        free(inflationBuffer);
    }
};

struct DeflationStream {
    z_stream deflationStream = {};

    DeflationStream(int compressOptions) {

        /* Sliding inflator should be about 44kb by default, less than compressor */

        /* Memory usage is given by 2 ^ (windowBits + 2) + 2 ^ (memLevel + 9) */
        int windowBits = -15, memLevel = 8;

        if (compressOptions == DEDICATED_COMPRESSOR_3KB) {
            windowBits = -9;
            memLevel = 1;
        } else if (compressOptions == DEDICATED_COMPRESSOR_4KB) {
            windowBits = -9;
            memLevel = 2;
        } else if (compressOptions == DEDICATED_COMPRESSOR_8KB) {
            windowBits = -10;
            memLevel = 3;
        } else if (compressOptions == DEDICATED_COMPRESSOR_16KB) {
            windowBits = -11;
            memLevel = 4;
        } else if (compressOptions == DEDICATED_COMPRESSOR_32KB) {
            windowBits = -12;
            memLevel = 5;
        } else if (compressOptions == DEDICATED_COMPRESSOR_64KB) {
            windowBits = -13;
            memLevel = 6;
        } else if (compressOptions == DEDICATED_COMPRESSOR_128KB) {
            windowBits = -14;
            memLevel = 7;
        } else if (compressOptions == DEDICATED_COMPRESSOR_256KB) {
            windowBits = -15;
            memLevel = 8;
        }

        /* DEDICATED_COMPRESSOR_256KB is the same as DEDICATED_COMPRESSOR */

        deflateInit2(&deflationStream, 1, Z_DEFLATED, windowBits, memLevel, Z_DEFAULT_STRATEGY);
    }

    /* Deflate and optionally reset */
    std::string_view deflate(ZlibContext *zlibContext, std::string_view raw, bool reset) {
        /* Odd place to clear this one, fix */
        zlibContext->dynamicDeflationBuffer.clear();

        deflationStream.next_in = (Bytef *) raw.data();
        deflationStream.avail_in = (unsigned int) raw.length();

        /* This buffer size has to be at least 6 bytes for Z_SYNC_FLUSH to work */
        const int DEFLATE_OUTPUT_CHUNK = LARGE_BUFFER_SIZE;

        int err;
        do {
            deflationStream.next_out = (Bytef *) zlibContext->deflationBuffer;
            deflationStream.avail_out = DEFLATE_OUTPUT_CHUNK;

            err = ::deflate(&deflationStream, Z_SYNC_FLUSH);
            if (Z_OK == err && deflationStream.avail_out == 0) {
                zlibContext->dynamicDeflationBuffer.append(zlibContext->deflationBuffer, DEFLATE_OUTPUT_CHUNK - deflationStream.avail_out);
                continue;
            } else {
                break;
            }
        } while (true);

        /* This must not change avail_out */
        if (reset) {
            deflateReset(&deflationStream);
        }

        if (zlibContext->dynamicDeflationBuffer.length()) {
            zlibContext->dynamicDeflationBuffer.append(zlibContext->deflationBuffer, DEFLATE_OUTPUT_CHUNK - deflationStream.avail_out);

            return {(char *) zlibContext->dynamicDeflationBuffer.data(), zlibContext->dynamicDeflationBuffer.length() - 4};
        }

        return {
            zlibContext->deflationBuffer,
            DEFLATE_OUTPUT_CHUNK - deflationStream.avail_out - 4
        };
    }

    ~DeflationStream() {
        deflateEnd(&deflationStream);
    }
};

struct InflationStream {
    z_stream inflationStream = {};

    InflationStream() {
        inflateInit2(&inflationStream, -15);
    }

    ~InflationStream() {
        inflateEnd(&inflationStream);
    }

    /* Zero length inflates are possible and valid */
    std::pair<std::string_view, bool> inflate(ZlibContext *zlibContext, std::string_view compressed, size_t maxPayloadLength) {

        /* We clear this one here, could be done better */
        zlibContext->dynamicInflationBuffer.clear();

        inflationStream.next_in = (Bytef *) compressed.data();
        inflationStream.avail_in = (unsigned int) compressed.length();

        int err;
        do {
            inflationStream.next_out = (Bytef *) zlibContext->inflationBuffer;
            inflationStream.avail_out = LARGE_BUFFER_SIZE;

            err = ::inflate(&inflationStream, Z_SYNC_FLUSH);
            if (err == Z_OK && inflationStream.avail_out) {
                break;
            }

            zlibContext->dynamicInflationBuffer.append(zlibContext->inflationBuffer, LARGE_BUFFER_SIZE - inflationStream.avail_out);


        } while (inflationStream.avail_out == 0 && zlibContext->dynamicInflationBuffer.length() <= maxPayloadLength);

        inflateReset(&inflationStream);

        if ((err != Z_BUF_ERROR && err != Z_OK) || zlibContext->dynamicInflationBuffer.length() > maxPayloadLength) {
            return {{nullptr, 0}, false};
        }

        if (zlibContext->dynamicInflationBuffer.length()) {
            zlibContext->dynamicInflationBuffer.append(zlibContext->inflationBuffer, LARGE_BUFFER_SIZE - inflationStream.avail_out);

            /* Let's be strict about the max size */
            if (zlibContext->dynamicInflationBuffer.length() > maxPayloadLength) {
                return {{nullptr, 0}, false};
            }

            return {zlibContext->dynamicInflationBuffer.data(), zlibContext->dynamicInflationBuffer.length()};
        }

        /* Let's be strict about the max size */
        if ((LARGE_BUFFER_SIZE - inflationStream.avail_out) > maxPayloadLength) {
            return {{nullptr, 0}, false};
        }

        return {{zlibContext->inflationBuffer, LARGE_BUFFER_SIZE - inflationStream.avail_out}, true};
    }

};

#endif

}

#endif // UWS_PERMESSAGEDEFLATE_H
