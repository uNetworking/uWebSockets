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

#ifndef UWS_ASYNCSOCKETDATA_H
#define UWS_ASYNCSOCKETDATA_H

#include <string>

namespace uWS {

/* Depending on how we want AsyncSocket to function, this will need to change */

template <bool SSL>
struct AsyncSocketData {
    /* This will do for now */
    std::string buffer;

    /* Allow move constructing us */
    AsyncSocketData(std::string &&backpressure) : buffer(std::move(backpressure)) {

    }

    /* Or emppty */
    AsyncSocketData() = default;
};

}

#endif // UWS_ASYNCSOCKETDATA_H
