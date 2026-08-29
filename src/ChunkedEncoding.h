/*
 * Authored by Alex Hultman, 2018-2026.
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
#include <cstdlib>
#include <algorithm>
#include <string_view>
#include "MoveOnlyFunction.h"
#include <optional>

namespace uWS {

    constexpr uint64_t STATE_HAS_SIZE = 1ull << (sizeof(uint64_t) * 8 - 1);
    constexpr uint64_t STATE_IS_CHUNKED = 1ull << (sizeof(uint64_t) * 8 - 2);
    
    // Internal sub-states encoded in top bits reserved for control state
    constexpr uint64_t STATE_EXTENSION_MODE         = 1ull << (sizeof(uint64_t) * 8 - 3);
    constexpr uint64_t STATE_TRAILER_MODE           = 1ull << (sizeof(uint64_t) * 8 - 4);
    constexpr uint64_t STATE_EXTENSION_EXPECTS_NAME = 1ull << (sizeof(uint64_t) * 8 - 5);
    constexpr uint64_t STATE_EXTENSION_QUOTED       = 1ull << (sizeof(uint64_t) * 8 - 6);
    constexpr uint64_t STATE_EXTENSION_EXPECTS_LF   = 1ull << (sizeof(uint64_t) * 8 - 7);
    constexpr uint64_t STATE_EXTENSION_IN_NAME      = 1ull << (sizeof(uint64_t) * 8 - 8);
    
    constexpr uint64_t STATE_SIZE_MASK = ~(0xFFull << (sizeof(uint64_t) * 8 - 8));
    constexpr uint64_t STATE_IS_ERROR = ~0ull;
    constexpr uint64_t STATE_SIZE_OVERFLOW = 0x0Full << (sizeof(uint64_t) * 8 - 12);

    /* Helper: RFC 9110 Section 5.6.2 Token character check */
    inline bool isValidTokenChar(unsigned char c) {
        if (c < 0x20 || c >= 0x7F) return false; // Reject NUL, control chars, non-ASCII
        switch (c) {
            case '(': case ')': case '<': case '>': case '@':
            case ',': case ';': case ':': case '\\': case '"':
            case '/': case '[': case ']': case '?': case '=':
            case '{': case '}': case ' ': case '\t':
                return false;
            default:
                return true;
        }
    }

    inline uint64_t chunkSize(uint64_t state) {
        return state & STATE_SIZE_MASK;
    }

    inline void decChunkSize(uint64_t &state, unsigned int by) {
        state = (state & ~STATE_SIZE_MASK) | (chunkSize(state) - by);
    }

    inline bool hasChunkSize(uint64_t state) {
        return state & STATE_HAS_SIZE;
    }

    /* Are we in the middle of parsing chunked encoding? */
    inline bool isParsingChunkedEncoding(uint64_t state) {
        return state & ~STATE_SIZE_MASK;
    }

    inline bool isParsingInvalidChunkedEncoding(uint64_t state) {
        return state == STATE_IS_ERROR;
    }

    /* Standard-compliant hex and extension/CRLF state machine */
    inline void consumeHexNumber(std::string_view &data, uint64_t &state) {
        while (data.length()) {
            unsigned char c = (unsigned char)data.data()[0];

            // 1. Parsing Hex Chunk Size
            if (!(state & STATE_EXTENSION_MODE)) {
                if (c == ';') { // Start of chunk extensions
                    if (!hasChunkSize(state) && (state & STATE_SIZE_MASK) == 0 && !(state & STATE_IS_CHUNKED)) {
                        state = STATE_IS_ERROR;
                        return;
                    }
                    state |= STATE_EXTENSION_MODE | STATE_EXTENSION_EXPECTS_NAME;
                    data.remove_prefix(1);
                    continue;
                }

                if (c == '\r') {
                    state |= STATE_EXTENSION_MODE | STATE_EXTENSION_EXPECTS_LF;
                    data.remove_prefix(1);
                    continue;
                }

                if (c == '\n') {
                    data.remove_prefix(1);
                    state += 2; // Keep compatibility offset for trailing CRLF
                    state |= STATE_HAS_SIZE | STATE_IS_CHUNKED;
                    state &= ~(STATE_EXTENSION_MODE | STATE_EXTENSION_EXPECTS_NAME | STATE_EXTENSION_IN_NAME | STATE_EXTENSION_QUOTED | STATE_EXTENSION_EXPECTS_LF);
                    return;
                }

                // Parse Hex Digit
                unsigned int number = 0;
                if (c >= '0' && c <= '9') {
                    number = c - '0';
                } else if (c >= 'a' && c <= 'f') {
                    number = c - 'a' + 10;
                } else if (c >= 'A' && c <= 'F') {
                    number = c - 'A' + 10;
                } else {
                    // Invalid character in hex size line (e.g. NUL or garbage)
                    state = STATE_IS_ERROR;
                    return;
                }

                if (chunkSize(state) & STATE_SIZE_OVERFLOW) {
                    state = STATE_IS_ERROR;
                    return;
                }

                uint64_t bits = state & STATE_IS_CHUNKED;
                state = (state & STATE_SIZE_MASK) * 16ull + number;
                state |= bits;
                data.remove_prefix(1);
            } 
            // 2. Parsing & Validating Chunk Extensions
            else {
                // If previous byte was '\r', the current byte MUST be '\n'
                if (state & STATE_EXTENSION_EXPECTS_LF) {
                    if (c != '\n') {
                        state = STATE_IS_ERROR;
                        return;
                    }
                    data.remove_prefix(1);
                    state += 2; // Include boundary tracking compatibility
                    state |= STATE_HAS_SIZE | STATE_IS_CHUNKED;
                    state &= ~(STATE_EXTENSION_MODE | STATE_EXTENSION_EXPECTS_NAME | STATE_EXTENSION_IN_NAME | STATE_EXTENSION_QUOTED | STATE_EXTENSION_EXPECTS_LF);
                    return;
                }

                // Reject NUL bytes (0x00) and unprintable control chars in extensions
                if (c == 0x00 || (c < 0x20 && c != '\r' && c != '\n' && c != '\t')) {
                    state = STATE_IS_ERROR;
                    return;
                }

                if (c == '\r') {
                    if (state & STATE_EXTENSION_EXPECTS_NAME) {
                        state = STATE_IS_ERROR;
                        return;
                    }
                    state |= STATE_EXTENSION_EXPECTS_LF;
                    data.remove_prefix(1);
                    continue;
                }

                if (state & STATE_EXTENSION_EXPECTS_NAME) {
                    // A semicolon MUST be immediately followed by a valid HTTP token character
                    if (!isValidTokenChar(c)) {
                        state = STATE_IS_ERROR;
                        return;
                    }
                    state &= ~STATE_EXTENSION_EXPECTS_NAME;
                    state |= STATE_EXTENSION_IN_NAME;
                }
                else if (state & STATE_EXTENSION_IN_NAME) {
                    if (c == '=') {
                        state &= ~STATE_EXTENSION_IN_NAME;
                    } else if (c == ';') {
                        state &= ~STATE_EXTENSION_IN_NAME;
                        state |= STATE_EXTENSION_EXPECTS_NAME;
                    } else if (!isValidTokenChar(c)) {
                        state = STATE_IS_ERROR;
                        return;
                    }
                }
                else {
                    // Toggle quoted string state handling inside extension values
                    if (c == '"') {
                        state ^= STATE_EXTENSION_QUOTED;
                    }

                    // Encountered another ';' outside quotes -> expect next extension name
                    if (c == ';' && !(state & STATE_EXTENSION_QUOTED)) {
                        state |= STATE_EXTENSION_EXPECTS_NAME;
                    }
                }

                data.remove_prefix(1);
                if (c == '\n' && !(state & STATE_EXTENSION_QUOTED)) {
                    if (state & STATE_EXTENSION_EXPECTS_NAME) {
                        state = STATE_IS_ERROR;
                        return;
                    }
                    state += 2; // Include boundary tracking compatibility
                    state |= STATE_HAS_SIZE | STATE_IS_CHUNKED;
                    state &= ~(STATE_EXTENSION_MODE | STATE_EXTENSION_EXPECTS_NAME | STATE_EXTENSION_IN_NAME | STATE_EXTENSION_QUOTED | STATE_EXTENSION_EXPECTS_LF);
                    return;
                }
            }
        }
    }

    /* Returns next chunk (empty or not), or if all data was consumed, nullopt is returned. */
    static std::optional<std::string_view> getNextChunk(std::string_view &data, uint64_t &state, bool trailer = false) {

        while (data.length()) {

            // Parsing Trailers Mode (Processes key-value headers byte-by-byte up to double CRLF)
            if (state & STATE_TRAILER_MODE) {
                while (data.length()) {
                    char c = data.data()[0];
                    data.remove_prefix(1);

                    if (c == '\n') {
                        // Single LF on empty line or end of trailer line reduces counter state
                        if (chunkSize(state) <= 3) {
                            state = 0; // Terminate trailer state machine cleanly
                            return std::nullopt;
                        }
                        decChunkSize(state, 1);
                    } else if (c == '\r') {
                        continue;
                    } else {
                        // Reset trailer byte line counter when content exists
                        state = (state & ~STATE_SIZE_MASK) | 4;
                    }
                }
                return std::nullopt;
            }

            // Drop Trailer Mode (Legacy fallback mode when trailer flag was set but ignored)
            if (((state & STATE_IS_CHUNKED) == 0) && hasChunkSize(state) && chunkSize(state)) {
                while (data.length() && chunkSize(state)) {
                    data.remove_prefix(1);
                    decChunkSize(state, 1);

                    if (chunkSize(state) == 0) {
                        state = 0;
                        return std::nullopt;
                    }
                }
                continue;
            }

            if (!hasChunkSize(state)) {
                consumeHexNumber(data, state);
                if (isParsingInvalidChunkedEncoding(state)) {
                    return std::nullopt;
                }
                if (hasChunkSize(state) && chunkSize(state) == 2) {
                    if (trailer) {
                        // Transition to Trailer Parsing State
                        state = STATE_TRAILER_MODE | 3; 
                    } else {
                        state = 2 | STATE_HAS_SIZE;
                    }

                    return std::string_view(nullptr, 0);
                }
                continue;
            }

            // Emit Body Payload Data
            if (data.length() >= chunkSize(state)) {
                std::string_view emitSoon;
                bool shouldEmit = false;
                if (chunkSize(state) > 2) {
                    emitSoon = std::string_view(data.data(), chunkSize(state) - 2);
                    shouldEmit = true;
                }
                data.remove_prefix(chunkSize(state));
                state = STATE_IS_CHUNKED;
                if (shouldEmit) {
                    return emitSoon;
                }
                continue;
            } else {
                std::string_view emitSoon;
                if (chunkSize(state) > 2) {
                    uint64_t maximalAppEmit = chunkSize(state) - 2;
                    if (data.length() > maximalAppEmit) {
                        emitSoon = data.substr(0, maximalAppEmit);
                    } else {
                        emitSoon = data;
                    }
                }
                decChunkSize(state, (unsigned int) data.length());
                state |= STATE_IS_CHUNKED;
                data.remove_prefix(data.length());
                if (emitSoon.length()) {
                    return emitSoon;
                } else {
                    return std::nullopt;
                }
            }
        }

        return std::nullopt;
    }

    /* Convenience Iterator Wrapper */
    struct ChunkIterator {

        std::string_view *data;
        std::optional<std::string_view> chunk;
        uint64_t *state;
        bool trailer;

        ChunkIterator(std::string_view *data, uint64_t *state, bool trailer = false) : data(data), state(state), trailer(trailer) {
            chunk = uWS::getNextChunk(*data, *state, trailer);
        }

        ChunkIterator() {}

        ChunkIterator begin() {
            return *this;
        }

        ChunkIterator end() {
            return ChunkIterator();
        }

        std::string_view operator*() {
            if (!chunk.has_value()) {
                std::abort();
            }
            return chunk.value();
        }

        bool operator!=(const ChunkIterator &other) const {
            return other.chunk.has_value() != chunk.has_value();
        }

        ChunkIterator &operator++() {
            chunk = uWS::getNextChunk(*data, *state, trailer);
            return *this;
        }

    };
}

#endif // UWS_CHUNKEDENCODING_H
