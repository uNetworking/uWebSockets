/*
 * Authored by Alex Hultman, 2018-2021.
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

#ifndef UWS_ASYNCSOCKETDATA_H
#define UWS_ASYNCSOCKETDATA_H

#include <string>

namespace uWS {

/* We erase a minimum of 32kb off the front of the backpressure buffer when draining */
const int BACKPRESSURE_MINIMAL_ERASE = 32 * 1024;

struct BackPressure {
    std::string buffer;
    unsigned int pendingRemoval = 0;
    BackPressure(BackPressure &&other) {
        buffer = std::move(other.buffer);
        pendingRemoval = other.pendingRemoval;
    }
    BackPressure() = default;
    void append(const char *data, size_t length) {
        buffer.append(data, length);
    }
    void erase(unsigned int length) {
        pendingRemoval += length;
        if (pendingRemoval > BACKPRESSURE_MINIMAL_ERASE || !(buffer.length() - pendingRemoval)) {
            buffer.erase(0, pendingRemoval);
            pendingRemoval = 0;
        }
    }
    size_t length() {
        return buffer.length() - pendingRemoval;
    }
    void clear() {
        pendingRemoval = 0;
        buffer.clear();
    }
    void reserve(size_t length) {
        buffer.reserve(length + pendingRemoval);
    }
    void resize(size_t length) {
        buffer.resize(length + pendingRemoval);
    }
    const char *data() {
        return buffer.data() + pendingRemoval;
    }
    size_t size() {
        return length();
    }
};

/* Depending on how we want AsyncSocket to function, this will need to change */

template <bool SSL>
struct AsyncSocketData {
    /* This will do for now */
    BackPressure buffer;

    /* Allow move constructing us */
    AsyncSocketData(BackPressure &&backpressure) : buffer(std::move(backpressure)) {

    }

    /* Or emppty */
    AsyncSocketData() = default;
};

}

#endif // UWS_ASYNCSOCKETDATA_H
