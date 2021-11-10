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

#ifndef UWS_BLOOMFILTER_H
#define UWS_BLOOMFILTER_H

/* This filter has a decently low amount of false positives for the
 * standard and non-standard common request headers */

#include <string_view>
#include <bitset>

namespace uWS {

struct BloomFilter {
private:
    std::bitset<512> filter;

    unsigned int hash1(std::string_view key) {
        return ((size_t)key[key.length() - 1] - (key.length() << 3)) & 511;
    }

    unsigned int hash2(std::string_view key) {
        return (((size_t)key[0] + (key.length() << 4)) & 511);
    }

    unsigned int hash3(std::string_view key) {
        return ((unsigned int)key[key.length() - 2] - 97 - (key.length() << 5)) & 511;
    }

public:
    bool mightHave(std::string_view key) {
        return filter.test(hash1(key)) && filter.test(hash2(key)) && (key.length() < 2 || filter.test(hash3(key)));
    }

    void add(std::string_view key) {
        filter.set(hash1(key));
        filter.set(hash2(key));
        if (key.length() >= 2) {
            filter.set(hash3(key));
        }
    }

    void reset() {
        filter.reset();
    }
};

}

#endif // UWS_BLOOMFILTER_H