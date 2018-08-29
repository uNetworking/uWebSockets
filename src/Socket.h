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

};

#endif // SOCKET_H
