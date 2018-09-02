#ifndef SOCKET_H
#define SOCKET_H

#include "libusockets.h"
#include <type_traits>

template <bool SSL>
struct Socket {

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


    void *getSocketContextExt() {
        SOCKET_CONTEXT_TYPE *socketContext = static_dispatch(us_ssl_socket_get_context, us_socket_get_context)((SOCKET_TYPE *) this);

        return static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)(socketContext);
    }

};

#endif // SOCKET_H
