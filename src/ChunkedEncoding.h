/*
 * Authored by Alex Hultman, 2018-2022.
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

#ifndef UWS_CHUNKEDENCODING_H
#define UWS_CHUNKEDENCODING_H

/* Independent chunked encoding parser, used by HttpParser. */

#include <string>
#include <cstring>
#include <algorithm>
#include <string_view>
#include "MoveOnlyFunction.h"
#include <optional>

namespace uWS {

    uint32_t STATE_HAS_SIZE = 0x80000000;
    uint32_t STATE_IS_CHUNKED = 0x40000000;
    uint32_t STATE_SIZE_MASK = 0x3FFFFFFF;

    /* Reads hex number until CR or out of data to consume. Updates state. Returns bytes consumed. */
    void consumeHexNumber(std::string_view &data, unsigned int &state) {
        /* Consume everything higher than 32 */
        while (data.length() && data.data()[0] > 32) {

            unsigned char digit = data.data()[0];
            if (digit >= 'a') {
                digit -= ('a' - '9') - 1;
            }

            state = state * 16u + ((unsigned int) digit - (unsigned int) '0');
            data.remove_prefix(1);
        }
        /* Consume everything not /n */
        while (data.length() && data.data()[0] != '\n') {
            data.remove_prefix(1);
        }
        /* Now we stand on \n so consume it and enable size */
        if (data.length()) {
            //printf("size of chunk is %d\n", state);
            state += 2; // include the two last /r/n
            state |= STATE_HAS_SIZE;
            data.remove_prefix(1);
        }
    }

    unsigned int chunkSize(unsigned int state) {
        return state & STATE_SIZE_MASK;
    }

    void decChunkSize(unsigned int &state, unsigned int by) {
        state = (state & ~STATE_SIZE_MASK) | (chunkSize(state) - by);
    }

    bool hasChunkSize(unsigned int state) {
        return state & STATE_HAS_SIZE;
    }

    // bättre interface
    std::optional<std::string_view> getNextChunk(std::string_view data, unsigned int &state);

    /* Consumes as much as possible, emitting chunks using cb. Changes passed state for later resumption. Returns number of bytes consumed. */
    inline unsigned int consumeChunkedEncoding(std::string_view data, unsigned int &state, MoveOnlyFunction<void(std::string_view)> cb) {

        std::string_view originalData = data;

        while (data.length()) {

            // if in "drop trailer mode", just drop up to what we have as size
            if ((state & STATE_IS_CHUNKED) && hasChunkSize(state) && chunkSize(state)) {

                while(data.length() && chunkSize(state)) {
                    //printf("dropping 1 byte of trailer\n");
                    data.remove_prefix(1);
                    decChunkSize(state, 1);

                    if (chunkSize(state) == 0) {
                        //printf("dropped the whole trailer\n");
                        state = 0;
                    }
                }
                continue;
            }

            if (!hasChunkSize(state)) {
                consumeHexNumber(data, state);
                if (hasChunkSize(state) && chunkSize(state) == 2) {

                    // set trailer state and increase size to 4
                    state = 4 | STATE_IS_CHUNKED | STATE_HAS_SIZE;

                    cb(std::string_view(nullptr, 0));
                }
                continue;
            }

            // do we have data to emit all?
            if (data.length() >= chunkSize(state)) {
                // emit all but 2 bytes then reset state to 0 and goto beginning
                // not fin
                if (chunkSize(state) > 2) {
                    cb(std::string_view(data.data(), chunkSize(state) - 2));
                }
                data.remove_prefix(chunkSize(state));
                state = 0;
                continue;
            } else {
                /* We will consume all our input data */
                if (chunkSize(state) > 2) {
                    unsigned int maximalAppEmit = chunkSize(state) - 2;
                    if (data.length() > maximalAppEmit) {
                        cb(data.substr(0, maximalAppEmit));
                    } else {
                        cb(data);
                    }
                }
                decChunkSize(state, data.length());
                return originalData.length();
            }
        }

        return originalData.length() - data.length();
    }
}

#endif // UWS_CHUNKEDENCODING_H
