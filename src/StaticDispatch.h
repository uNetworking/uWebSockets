#ifndef STATICDISPATCH_H
#define STATICDISPATCH_H

/* This headers is basically a statically dispatched libusockets wrapper base */

#include <type_traits>
#include <libusockets.h>

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
