/*
 * Copyright 2018 Alex Hultman and contributors.

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

// inflationStream? Ciompression

#ifndef PERMESSAGEDEFLATE_H
#define PERMESSAGEDEFLATE_H
/* Do not compile this module if we don't want it */
#ifndef UWS_NO_ZLIB

#include <zlib.h>

#include <string>
#include <iostream>

#define LARGE_BUFFER_SIZE 16000 // fix this

// we also need DeflationStream

struct DeflationStream {

    // share this under the Loop
    std::string dynamicZlibBuffer;
    z_stream deflationStream = {};
    char *zlibBuffer;

    DeflationStream() {
        std::cout << "Constructing DeflationStream" << std::endl;
        zlibBuffer = (char *) malloc(LARGE_BUFFER_SIZE);

        deflateInit2(&deflationStream, 1, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
    }

    std::string_view deflate(std::string_view raw) {

        // slidingDeflateWindow är input, length är in/ut

        z_stream *slidingDeflateWindow = nullptr;

        dynamicZlibBuffer.clear();

        z_stream *compressor = slidingDeflateWindow ? slidingDeflateWindow : &deflationStream;

        compressor->next_in = (Bytef *) raw.data();
        compressor->avail_in = (unsigned int) raw.length();

        // note: zlib requires more than 6 bytes with Z_SYNC_FLUSH
        const int DEFLATE_OUTPUT_CHUNK = LARGE_BUFFER_SIZE;

        int err;
        do {
            compressor->next_out = (Bytef *) zlibBuffer;
            compressor->avail_out = DEFLATE_OUTPUT_CHUNK;

            err = ::deflate(compressor, Z_SYNC_FLUSH);
            if (Z_OK == err && compressor->avail_out == 0) {
                dynamicZlibBuffer.append(zlibBuffer, DEFLATE_OUTPUT_CHUNK - compressor->avail_out);
                continue;
            } else {
                break;
            }
        } while (true);

        // note: should not change avail_out
        if (!slidingDeflateWindow) {
            deflateReset(compressor);
        }

        if (dynamicZlibBuffer.length()) {
            dynamicZlibBuffer.append(zlibBuffer, DEFLATE_OUTPUT_CHUNK - compressor->avail_out);

            return {(char *) dynamicZlibBuffer.data(), dynamicZlibBuffer.length() - 4};

            //length = dynamicZlibBuffer.length() - 4;
            //return (char *) dynamicZlibBuffer.data();
        }

        return {
            zlibBuffer,
            DEFLATE_OUTPUT_CHUNK - compressor->avail_out - 4
        };

        //length = DEFLATE_OUTPUT_CHUNK - compressor->avail_out - 4;
        //return zlibBuffer;
    }

    ~DeflationStream() {
        std::cout << "Destructing DeflationStream" << std::endl;
    }
};

// the loop holds one of these
struct InflationStream {

    // share this under the Loop
    std::string dynamicZlibBuffer;
    z_stream inflationStream = {};
    char *zlibBuffer;

    InflationStream() {
        std::cout << "Initliazing shared compression" << std::endl;
        zlibBuffer = (char *) malloc(LARGE_BUFFER_SIZE);

        inflateInit2(&inflationStream, -15);
    }

    std::string_view inflate(std::string_view compressed) {

        int maxPayload = 160000; // todo: fix this

        dynamicZlibBuffer.clear();

        inflationStream.next_in = (Bytef *) compressed.data();
        inflationStream.avail_in = (unsigned int) compressed.length();

        int err;
        do {
            inflationStream.next_out = (Bytef *) zlibBuffer;
            inflationStream.avail_out = LARGE_BUFFER_SIZE;
            err = ::inflate(&inflationStream, Z_FINISH);
            if (!inflationStream.avail_in) {
                break;
            }

            dynamicZlibBuffer.append(zlibBuffer, LARGE_BUFFER_SIZE - inflationStream.avail_out);
        } while (err == Z_BUF_ERROR && dynamicZlibBuffer.length() <= maxPayload);

        inflateReset(&inflationStream);

        if ((err != Z_BUF_ERROR && err != Z_OK) || dynamicZlibBuffer.length() > maxPayload) {
            return {nullptr, 0};
        }

        if (dynamicZlibBuffer.length()) {
            dynamicZlibBuffer.append(zlibBuffer, LARGE_BUFFER_SIZE - inflationStream.avail_out);
            return {dynamicZlibBuffer.data(), dynamicZlibBuffer.length()};
        }

        return {zlibBuffer, LARGE_BUFFER_SIZE - inflationStream.avail_out};
    }

};

#endif
#endif // PERMESSAGEDEFLATE_H
