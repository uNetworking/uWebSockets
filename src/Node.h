#ifndef NODE_UWS_H
#define NODE_UWS_H

#include "Socket.h"
#include <vector>
#include <mutex>

namespace uS {

enum ListenOptions : int {
    REUSE_PORT = 1,
    ONLY_IPV4 = 2
};

class WIN32_EXPORT Node {
protected:
    Loop *loop;
    NodeData *nodeData;
    std::mutex asyncMutex;

public:
    Node(int recvLength = 1024, int prePadding = 0, int postPadding = 0, bool useDefaultLoop = false);
    ~Node();
    void run();

    Loop *getLoop() {
        return loop;
    }

    template <void C(Socket *p, bool error)>
    static void connect_cb(Poll *p, int status, int events) {
        C((Socket *) p, status < 0);
    }

    template <uS::Socket *I(Socket *s), void C(Socket *p, bool error)>
    Socket *connect(const char *hostname, int port, bool secure, NodeData *nodeData) {
        addrinfo hints, *result;
        memset(&hints, 0, sizeof(addrinfo));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(hostname, std::to_string(port).c_str(), &hints, &result) != 0) {
            return nullptr;
        }

        uv_os_sock_t fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (fd == -1) {
            return nullptr;
        }

#ifdef __APPLE__
        int noSigpipe = 1;
        setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &noSigpipe, sizeof(int));
#endif

        ::connect(fd, result->ai_addr, result->ai_addrlen);
        freeaddrinfo(result);

        SSL *ssl = nullptr;
        if (secure) {
            ssl = SSL_new(nodeData->clientContext);
            SSL_set_connect_state(ssl);
            SSL_set_tlsext_host_name(ssl, hostname);
        }

        Socket initialSocket(nodeData, getLoop(), fd, ssl);
        uS::Socket *socket = I(&initialSocket);

        socket->setCb(connect_cb<C>);
        socket->start(loop, socket, socket->setPoll(UV_WRITABLE));
        return socket;
    }

    template <void A(Socket *s)>
    static void accept_poll_cb(Poll *p, int status, int events) {
        ListenData *listenData = (ListenData *) p;
        accept_cb<A, false>(listenData);
    }

    template <void A(Socket *s)>
    static void accept_timer_cb(Timer *p) {
        ListenData *listenData = (ListenData *) p->getData();
        accept_cb<A, true>(listenData);
    }

    template <void A(Socket *s), bool TIMER>
    static void accept_cb(ListenData *listenData) {
        uv_os_sock_t serverFd = listenData->sock;
        uv_os_sock_t clientFd = accept(serverFd, nullptr, nullptr);
//        if (clientFd == INVALID_SOCKET) {
//            /*
//            * If accept is failing, the pending connection won't be removed and the
//            * polling will cause the server to spin, using 100% cpu. Switch to a timer
//            * event instead to avoid this.
//            */
//            if (!TIMER && errno != EAGAIN && errno != EWOULDBLOCK) {
//                listenData->listenPoll->stop(listenData->nodeData->loop);
//                listenData->listenPoll->close(listenData->nodeData->loop);
//                listenData->listenPoll = nullptr;

//                listenData->listenTimer = new Timer(listenData->nodeData->loop);
//                listenData->listenTimer->setData(listenData);
//                listenData->listenTimer->start(accept_timer_cb<A>, 1000, 1000);
//            }
//            return;
//        } else if (TIMER) {
//            listenData->listenTimer->stop();
//            listenData->listenTimer->close();
//            listenData->listenTimer = nullptr;

//            // todo: broken
//            //listenData->listenPoll = new Poll(listenData->nodeData->loop, serverFd);
//            //listenData->listenPoll->setData(listenData);

//            listenData->listenPoll->setCb(accept_poll_cb<A>);
//            listenData->listenPoll->start(listenData->nodeData->loop, listenData, UV_READABLE);
//        }
        do {
#ifdef __APPLE__
            int noSigpipe = 1;
            setsockopt(clientFd, SOL_SOCKET, SO_NOSIGPIPE, &noSigpipe, sizeof(int));
#endif

            SSL *ssl = nullptr;
            if (listenData->sslContext) {
                ssl = SSL_new(listenData->sslContext.getNativeContext());
                SSL_set_accept_state(ssl);
            }

            Socket *socket = new Socket(listenData->nodeData, listenData->nodeData->loop, clientFd, ssl);
            socket->setPoll(UV_READABLE);
            A(socket);
        } while ((clientFd = accept(serverFd, nullptr, nullptr)) != INVALID_SOCKET);
    }

    // todo: hostname, backlog
    template <void A(Socket *s)>
    bool listen(const char *host, int port, uS::TLS::Context sslContext, int options, uS::NodeData *nodeData, void *user) {
        addrinfo hints, *result;
        memset(&hints, 0, sizeof(addrinfo));

        hints.ai_flags = AI_PASSIVE;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo(host, std::to_string(port).c_str(), &hints, &result)) {
            return true;
        }

        uv_os_sock_t listenFd = SOCKET_ERROR;
        addrinfo *listenAddr;
        if ((options & uS::ONLY_IPV4) == 0) {
            for (addrinfo *a = result; a && listenFd == SOCKET_ERROR; a = a->ai_next) {
                if (a->ai_family == AF_INET6) {
                    listenFd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
                    listenAddr = a;
                }
            }
        }

        for (addrinfo *a = result; a && listenFd == SOCKET_ERROR; a = a->ai_next) {
            if (a->ai_family == AF_INET) {
                listenFd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
                listenAddr = a;
            }
        }

        if (listenFd == SOCKET_ERROR) {
            freeaddrinfo(result);
            return true;
        }

#ifdef __linux
#ifdef SO_REUSEPORT
        if (options & REUSE_PORT) {
            int optval = 1;
            setsockopt(listenFd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
        }
#endif
#endif

        int enabled = true;
        setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));

        if (bind(listenFd, listenAddr->ai_addr, listenAddr->ai_addrlen) || ::listen(listenFd, 512)) {
            ::close(listenFd);
            freeaddrinfo(result);
            return true;
        }

        ListenData *listenData = new ListenData(nodeData, loop, listenFd, nullptr);
        listenData->sslContext = sslContext;
        listenData->nodeData = nodeData;

        listenData->setCb(accept_poll_cb<A>);
        listenData->start(loop, listenData, UV_READABLE);


        // why is this needed?
        listenData->sock = listenFd;

        listenData->ssl = nullptr;

        // should be vector of listen data! one group can have many listeners!
        nodeData->user = listenData;
        freeaddrinfo(result);
        return false;
    }
};

}

#endif // NODE_UWS_H
