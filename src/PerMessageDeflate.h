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

// the loop holds one of these
struct InflationStream {

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
