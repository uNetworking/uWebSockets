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

#include "MoveOnlyFunction.h"

#include <type_traits>

namespace uWS {

template <bool> struct HttpResponse;

template <bool SSL>
struct HttpResponseData : AsyncSocketData<SSL>, HttpParser {
    template <bool> friend struct HttpResponse;
    template <bool> friend struct HttpContext;

    /* When we are done with a response we mark it like so */
    void markDone() {
        onAborted = nullptr;
        /* Also remove onWritable so that we do not emit when draining behind the scenes. */
        onWritable = nullptr;
        /* The handlers that get the abort get nothing more, not even the rest of the body */
        onWritableOrAborted = nullptr;
        inStreamOrAborted = nullptr;

        /* We are done with this request */
        state &= ~HttpResponseData<SSL>::HTTP_RESPONSE_PENDING;
    }

    /* Caller of onWritable. It is possible onWritable calls markDone so we need to borrow it.
     * It is also possible user code sets a new onWritable while running user registered onWritable. */
    bool callOnWritable(uintmax_t offset) {
        return callBorrowed(onWritable, offset);
    }

    /* Callers of the handlers that get the abort, borrowed for the same reasons */
    bool callOnWritableOrAborted(HttpResponse<SSL> *res, uintmax_t offset) {
        return callBorrowed(onWritableOrAborted, res, offset);
    }

    void callInStreamOrAborted(HttpResponse<SSL> *res, std::string_view data, uint64_t maxRemainingBodyLength) {
        callBorrowed(inStreamOrAborted, res, data, maxRemainingBodyLength);
    }
private:
    template <typename R, typename... Args>
    static R callBorrowed(MoveOnlyFunction<R(Args...)> &slot, Args... args) {
        /* 1. Borrow the real callback */
        MoveOnlyFunction<R(Args...)> borrowed = std::move(slot);
    
        /* 2. Setup the stack-based detection flag */
        bool placeholderReplaced = false;
    
        struct Sentinel {
            bool *replacedFlag;
            Sentinel(bool *f) : replacedFlag(f) {}

            Sentinel(Sentinel &&other) noexcept : replacedFlag(other.replacedFlag) {
                other.replacedFlag = nullptr; 
            }
            
            ~Sentinel() {
                if (replacedFlag) {
                    *replacedFlag = true;
                }
            }
    
            /* Delete copy to ensure move-only semantics */
            Sentinel(const Sentinel&) = delete;
            Sentinel& operator=(const Sentinel&) = delete;
        };
    
        /* 3. Set placeholder with the captured Sentinel */
        slot = [tracker = Sentinel(&placeholderReplaced)](Args...) -> R {
            if constexpr (!std::is_void_v<R>) {
                return true;
            }
        };
    
        /* 4. Run the borrowed callback. 5. When done, if placeholderReplaced is STILL false, it means the lambda (and
           its Sentinel) is still sitting inside the slot, so put the callback back. If it's true, the lambda was
           destroyed to make room for a new one. */
        struct PutBack {
            MoveOnlyFunction<R(Args...)> &slot, &borrowed;
            bool &placeholderReplaced;
            ~PutBack() {
                if (!placeholderReplaced) {
                    slot = std::move(borrowed);
                }
            }
        } putBack{slot, borrowed, placeholderReplaced};

        return borrowed(args...);
    }

    /* Bits of status */
    enum {
        HTTP_STATUS_CALLED = 1, // used
        HTTP_WRITE_CALLED = 2, // used
        HTTP_END_CALLED = 4, // used
        HTTP_RESPONSE_PENDING = 8, // used
        HTTP_CONNECTION_CLOSE = 16 // used
    };

    /* Per socket event handlers */
    MoveOnlyFunction<bool(uintmax_t)> onWritable;
    MoveOnlyFunction<void()> onAborted;
    MoveOnlyFunction<void(std::string_view, uint64_t)> inStream; // onData
    /* onWritableOrAborted and onDataOrAborted: they get the response first, and the abort as a null response until
     * markDone. Each one replaces onWritable and onData, and the other way around */
    MoveOnlyFunction<bool(HttpResponse<SSL> *, uintmax_t)> onWritableOrAborted;
    MoveOnlyFunction<void(HttpResponse<SSL> *, std::string_view, uint64_t)> inStreamOrAborted;
    /* Outgoing offset */
    uintmax_t offset = 0;

    /* Let's track number of bytes since last timeout reset in data handler */
    unsigned int received_bytes_per_timeout = 0;

    /* Current state (content-length sent, status sent, write called, etc */
    int state = 0;

#ifdef UWS_WITH_PROXY
    ProxyParser proxyParser;
#endif
};

}

#endif // UWS_HTTPRESPONSEDATA_H
