/*
 * Authored by Alex Hultman, 2018-2025.
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

#include <cstring>
#include <string>

namespace uWS {

struct BackPressure {
    static constexpr size_t TRIM_THRESHOLD = 64 * 1024;
    static constexpr size_t RETAIN_CAPACITY = 32 * 1024;

    std::string buffer;
    unsigned int pendingRemoval = 0;

    void normalize() {
        size_t length = buffer.length() - pendingRemoval;
        if (length) {
            memmove(buffer.data(), buffer.data() + pendingRemoval, length);
        }
        buffer.resize(length);
        pendingRemoval = 0;
    }

    BackPressure(BackPressure &&other) {
        buffer = std::move(other.buffer);
        pendingRemoval = other.pendingRemoval;
    }
    BackPressure() = default;
    void append(const char *data, size_t length) {
        buffer.append(data, length);
    }
    void erase(unsigned int length) {
        size_t logicalLength = this->length();
        if (length >= logicalLength) {
            clear();
            return;
        }

        pendingRemoval += length;
        /* Always erase a minimum of 1/32th the current backpressure */
        if (pendingRemoval > (buffer.length() >> 5)) {
            normalize();
        }
    }
    size_t length() {
        return buffer.length() - pendingRemoval;
    }
    /* Only used in AsyncSocket::write when buffered backpressure fully drains */
    void clear() {
        pendingRemoval = 0;
        if (buffer.capacity() > TRIM_THRESHOLD) {
            /* Trim pathological spikes but keep a warm buffer for normal bursts */
            std::string retained;
            retained.reserve(RETAIN_CAPACITY);
            buffer.swap(retained);
        } else {
            buffer.clear();
        }
    }
    /* Only used by AsyncSocket::write (optionally) before append */
    void reserve(size_t length) {
        buffer.reserve(length + pendingRemoval);
    }
    /* Only used by getSendBuffer as last resort */
    void resize(size_t length) {
        if (pendingRemoval) {
            normalize();
        }
        buffer.resize(length);
    }
    const char *data() {
        return buffer.data() + pendingRemoval;
    }
    /* The total length, incuding pending removal */
    size_t totalLength() {
        return buffer.length();
    }
};

/* Depending on how we want AsyncSocket to function, this will need to change */

template <bool SSL>
struct AsyncSocketData {
    /* This will do for now */
    BackPressure buffer;

#ifdef UWS_REMOTE_ADDRESS_USERSPACE
    /* Cache for remote address, populated on socket open */
    char remoteAddress[16];
    int remoteAddressLength = 0;
#endif

    /* Allow move constructing us */
    AsyncSocketData(BackPressure &&backpressure) : buffer(std::move(backpressure)) {

    }

    /* Or emppty */
    AsyncSocketData() = default;
};

}

#endif // UWS_ASYNCSOCKETDATA_H
