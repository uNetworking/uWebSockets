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

#ifndef WEBSOCKETCONTEXTDATA_H
#define WEBSOCKETCONTEXTDATA_H

#include <functional>
#include <string_view>

#include "WebSocketProtocol.h"

#include "f2/function2.hpp"

namespace uWS {

template <bool, bool> struct WebSocket;

/* todo: this looks identical to WebSocketBehavior, why not just std::move that entire thing in? */

template <bool SSL>
struct WebSocketContextData {
    /* The callbacks for this context */
    fu2::unique_function<void(WebSocket<SSL, true> *, std::string_view, uWS::OpCode)> messageHandler = nullptr;
    fu2::unique_function<void(WebSocket<SSL, true> *)> drainHandler = nullptr;
    fu2::unique_function<void(WebSocket<SSL, true> *, int, std::string_view)> closeHandler = nullptr;

    /* Settings for this context */
    size_t maxPayloadLength = 0;
    int idleTimeout = 0;
};

}

#endif // WEBSOCKETCONTEXTDATA_H
