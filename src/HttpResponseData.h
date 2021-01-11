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

#ifndef UWS_HTTPRESPONSEDATA_H
#define UWS_HTTPRESPONSEDATA_H

/* This data belongs to the HttpResponse */

#include "HttpParser.h"
#include "AsyncSocketData.h"
#include "ProxyParser.h"

#include "f2/function2.hpp"

namespace uWS {

template <bool SSL>
struct HttpResponseData : AsyncSocketData<SSL>, HttpParser {
    template <bool> friend struct HttpResponse;
    template <bool> friend struct HttpContext;
private:
    /* Bits of status */
    enum {
        HTTP_STATUS_CALLED = 1, // used
        HTTP_WRITE_CALLED = 2, // used
        HTTP_END_CALLED = 4, // used
        HTTP_RESPONSE_PENDING = 8, // used
        HTTP_CONNECTION_CLOSE = 16 // used
    };

    /* Per socket event handlers */
    fu2::unique_function<bool(size_t)> onWritable;
    fu2::unique_function<void()> onAborted;
    fu2::unique_function<void(std::string_view, bool)> inStream; // onData
    /* Outgoing offset */
    size_t offset = 0;

    /* Current state (content-length sent, status sent, write called, etc */
    int state = 0;

#ifdef UWS_WITH_PROXY
    ProxyParser proxyParser;
#endif
};

}

#endif // UWS_HTTPRESPONSEDATA_H
