/*
 * Copyright 2018 Alex Hultman and contributors.

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

#ifndef STATICDISPATCH_H
#define STATICDISPATCH_H

/* This headers is basically a statically dispatched libusockets wrapper base */

#include <type_traits>
#include <libusockets.h>

/* For now we just define all these as nonsense if using a non-SSL version of uSockets */
#ifdef LIBUS_NO_SSL
#define us_ssl_socket_context_options int
#define us_ssl_socket_context_ext us_socket_context_ext
#define us_ssl_socket_context_on_open us_socket_context_on_open
#define us_ssl_socket_timeout us_socket_timeout
#define us_ssl_socket_ext us_socket_ext
#define us_ssl_socket_context_on_close us_socket_context_on_close
#define us_ssl_socket_context_on_data us_socket_context_on_data
#define us_ssl_socket_context_on_writable us_socket_context_on_writable
#define us_ssl_socket_context_on_end us_socket_context_on_end
#define us_ssl_socket_context_on_timeout us_socket_context_on_timeout
#define us_ssl_socket_is_shut_down us_socket_is_shut_down
#define us_ssl_socket_close us_socket_close
#define us_ssl_socket_write us_socket_write
#define us_ssl_socket_context_loop us_socket_context_loop
#define us_ssl_socket_get_context us_socket_get_context
#define us_ssl_socket_context_listen us_socket_context_listen
#define us_ssl_socket_context_adopt_socket us_socket_context_adopt_socket
#define us_create_child_ssl_socket_context us_create_child_socket_context
#define us_ssl_socket_shutdown us_socket_shutdown
#define us_ssl_socket_context_free us_socket_context_free
#endif

namespace uWS {

template <bool SSL>
struct StaticDispatch {
    template <class A, class B>
    static constexpr typename std::conditional<SSL, A, B>::type *static_dispatch(A *a, B *b) {
        if constexpr(SSL) {
            return a;
        } else {
            return b;
        }
    }

    typedef typename std::conditional<SSL, us_ssl_socket, us_socket>::type SOCKET_TYPE;
    typedef typename std::conditional<SSL, us_ssl_socket_context, us_socket_context>::type SOCKET_CONTEXT_TYPE;
};

}

#endif // STATICDISPATCH_H
