/*
 * Authored by Alex Hultman, 2018-2020.
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

#ifndef UWS_WEBSOCKETHANDSHAKE_H
#define UWS_WEBSOCKETHANDSHAKE_H

#include <cstdint>
#include <cstddef>
#include <sys/random.h>

namespace uWS {

// Shared Base64 character table (file-scope internal linkage)
static const char base64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

struct WebSocketHandshake {
    template <int N, typename T>
    struct static_for {
        void operator()(uint32_t *a, uint32_t *b) {
            static_for<N - 1, T>()(a, b);
            T::template f<N - 1>(a, b);
        }
    };

    template <typename T>
    struct static_for<0, T> {
        void operator()(uint32_t */*a*/, uint32_t */*hash*/) {}
    };

    static inline uint32_t rol(uint32_t value, size_t bits) {return (value << bits) | (value >> (32 - bits));}
    static inline uint32_t blk(uint32_t b[16], size_t i) {
        return rol(b[(i + 13) & 15] ^ b[(i + 8) & 15] ^ b[(i + 2) & 15] ^ b[i], 1);
    }

    struct Sha1Loop1 {
        template <int i>
        static inline void f(uint32_t *a, uint32_t *b) {
            a[i % 5] += ((a[(3 + i) % 5] & (a[(2 + i) % 5] ^ a[(1 + i) % 5])) ^ a[(1 + i) % 5]) + b[i] + 0x5a827999 + rol(a[(4 + i) % 5], 5);
            a[(3 + i) % 5] = rol(a[(3 + i) % 5], 30);
        }
    };
    struct Sha1Loop2 {
        template <int i>
        static inline void f(uint32_t *a, uint32_t *b) {
            b[i] = blk(b, i);
            a[(1 + i) % 5] += ((a[(4 + i) % 5] & (a[(3 + i) % 5] ^ a[(2 + i) % 5])) ^ a[(2 + i) % 5]) + b[i] + 0x5a827999 + rol(a[(5 + i) % 5], 5);
            a[(4 + i) % 5] = rol(a[(4 + i) % 5], 30);
        }
    };
    struct Sha1Loop3 {
        template <int i>
        static inline void f(uint32_t *a, uint32_t *b) {
            b[(i + 4) % 16] = blk(b, (i + 4) % 16);
            a[i % 5] += (a[(3 + i) % 5] ^ a[(2 + i) % 5] ^ a[(1 + i) % 5]) + b[(i + 4) % 16] + 0x6ed9eba1 + rol(a[(4 + i) % 5], 5);
            a[(3 + i) % 5] = rol(a[(3 + i) % 5], 30);
        }
    };
    struct Sha1Loop4 {
        template <int i>
        static inline void f(uint32_t *a, uint32_t *b) {
            b[(i + 8) % 16] = blk(b, (i + 8) % 16);
            a[i % 5] += (((a[(3 + i) % 5] | a[(2 + i) % 5]) & a[(1 + i) % 5]) | (a[(3 + i) % 5] & a[(2 + i) % 5])) + b[(i + 8) % 16] + 0x8f1bbcdc + rol(a[(4 + i) % 5], 5);
            a[(3 + i) % 5] = rol(a[(3 + i) % 5], 30);
        }
    };
    struct Sha1Loop5 {
        template <int i>
        static inline void f(uint32_t *a, uint32_t *b) {
            b[(i + 12) % 16] = blk(b, (i + 12) % 16);
            a[i % 5] += (a[(3 + i) % 5] ^ a[(2 + i) % 5] ^ a[(1 + i) % 5]) + b[(i + 12) % 16] + 0xca62c1d6 + rol(a[(4 + i) % 5], 5);
            a[(3 + i) % 5] = rol(a[(3 + i) % 5], 30);
        }
    };
    struct Sha1Loop6 {
        template <int i>
        static inline void f(uint32_t *a, uint32_t *b) {
            b[i] += a[4 - i];
        }
    };

    static inline void sha1(uint32_t hash[5], uint32_t b[16]) {
        uint32_t a[5] = {hash[4], hash[3], hash[2], hash[1], hash[0]};
        static_for<16, Sha1Loop1>()(a, b);
        static_for<4, Sha1Loop2>()(a, b);
        static_for<20, Sha1Loop3>()(a, b);
        static_for<20, Sha1Loop4>()(a, b);
        static_for<20, Sha1Loop5>()(a, b);
        static_for<5, Sha1Loop6>()(a, hash);
    }

    // Base64-encode an arbitrary length (multiple of 3) chunk
    static inline void base64Encode(const unsigned char *src, size_t len, char *dst) {
        size_t i = 0;
        for (; i + 2 < len; i += 3) {
            unsigned v = (src[i] << 16) | (src[i+1] << 8) | src[i+2];
            *dst++ = base64_table[v >> 18];
            *dst++ = base64_table[(v >> 12) & 0x3F];
            *dst++ = base64_table[(v >> 6)  & 0x3F];
            *dst++ = base64_table[v & 0x3F];
        }
        // handle padding
        if (i < len) {
            unsigned v = src[i] << 16;
            if (i+1 < len) v |= src[i+1] << 8;
            *dst++ = base64_table[v >> 18];
            *dst++ = base64_table[(v >> 12) & 0x3F];
            *dst++ = (i+1 < len) ? base64_table[(v >> 6) & 0x3F] : '=';
            *dst++ = '=';
        }
    }

    // Fill buffer with randomness via getrandom()
    static inline bool fillRandom(unsigned char *buf, size_t len) {
        size_t got = 0;
        while (got < len) {
            ssize_t n = getrandom(buf + got, len - got, 0);
            if (n < 0) {
                if (errno == EINTR) continue;
                return false;
            }
            got += static_cast<size_t>(n);
        }
        return true;
    }

public:
    static inline void generate(const char input[24], char output[28]) {
        uint32_t b_output[5] = {
            0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0
        };
        uint32_t b_input[16] = {
            0, 0, 0, 0, 0, 0, 0x32353845, 0x41464135, 0x2d453931, 0x342d3437, 0x44412d39,
            0x3543412d, 0x43354142, 0x30444338, 0x35423131, 0x80000000
        };

        for (int i = 0; i < 6; i++) {
            b_input[i] = (uint32_t) ((input[4 * i + 3] & 0xff) | (input[4 * i + 2] & 0xff) << 8 | (input[4 * i + 1] & 0xff) << 16 | (input[4 * i + 0] & 0xff) << 24);
        }
        sha1(b_output, b_input);
        uint32_t last_b[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 480};
        sha1(b_output, last_b);
        for (int i = 0; i < 5; i++) {
            uint32_t tmp = b_output[i];
            char *bytes = (char *) &b_output[i];
            bytes[3] = (char) (tmp & 0xff);
            bytes[2] = (char) ((tmp >> 8) & 0xff);
            bytes[1] = (char) ((tmp >> 16) & 0xff);
            bytes[0] = (char) ((tmp >> 24) & 0xff);
        }
        base64Encode((unsigned char *) b_output, 20, output);
    }

    /**
     * Generate a new Sec-WebSocket-Key: random 16 bytes → 24-char Base64
     * output must be at least 24 chars (no null terminator added)
     * returns true on success
     */
    static inline bool generateKey(unsigned char key[16], char output[24]) {
        if (!fillRandom(key, 16)) {
            return false;
        }
        base64Encode(key, 16, output);
        return true;
    }
    
    static inline bool generateKey(char output[24]) {
        unsigned char key[16];
        if (!fillRandom(key, 16)) {
            return false;
        }
        base64Encode(key, 16, output);
        return true;
    }
};

}

#endif // UWS_WEBSOCKETHANDSHAKE_H
