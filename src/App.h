#ifndef CONTEXT_H
#define CONTEXT_H

#include "libusockets.h"
#include <type_traits>
#include "Loop.h"
#include "HttpSocket.h"
#include "HttpRouter.h"

namespace uWS {

template <bool SSL>
class AppBase {

protected:

    static const unsigned int HTTP_IDLE_TIMEOUT_S = 10;

    template <class A, class B>
    static constexpr typename std::conditional<SSL, A, B>::type *static_dispatch(A *a, B *b) {
        if constexpr(SSL) {
            return a;
        } else {
            return b;
        }
    }

    typedef typename std::conditional<SSL, us_ssl_socket_context, us_socket_context>::type SOCKET_CONTEXT_TYPE;
    typedef typename HttpSocket<SSL>::Data HTTP_SOCKET_DATA_TYPE;

    struct Data {
        Data() {

            // default http handler is a router
            onHttpRequest = [this](auto *s, HttpRequest *req) {
                UserData user = {s, req};
                r.route("GET", 3, req->getUrl().data(), req->getUrl().length(), &user);
            };

        }

        struct UserData {
            HttpSocket<SSL> *httpSocket;
            HttpRequest *httpRequest;
        };

        HttpRouter<UserData *> r;

        std::function<void(HttpSocket<SSL> *)> onHttpConnection;
        std::function<void(HttpSocket<SSL> *)> onHttpDisconnection;
        std::function<void(HttpSocket<SSL> *, HttpRequest *)> onHttpRequest;
    } *data;

    // server protocols
    SOCKET_CONTEXT_TYPE *httpServerContext;
    SOCKET_CONTEXT_TYPE *webSocketServerContext;

    // a us_socket_context can be constructed from another us_socket_context and will then act purely as a behavior (shared SSL_CTX)
    // only socket contexts that come from construction from the start can be listened or connected to

    // us_create_derived_socket_context(us_socket_context)

    // client protocols

    void init(us_loop *loop) {
            new (data = (Data *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)(httpServerContext)) Data();

            static_dispatch(us_ssl_socket_context_on_open, us_socket_context_on_open)(httpServerContext, [](auto *s) {
                Data *appData = (Data *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)(static_dispatch(us_ssl_socket_get_context, us_socket_get_context)(s));

                static_dispatch(us_ssl_socket_timeout, us_socket_timeout)(s, HTTP_IDLE_TIMEOUT_S);

                new (static_dispatch(us_ssl_socket_ext, us_socket_ext)(s)) HTTP_SOCKET_DATA_TYPE;

                if (appData->onHttpConnection) {
                    appData->onHttpConnection((HttpSocket<SSL> *) s);
                }
            });

            static_dispatch(us_ssl_socket_context_on_close, us_socket_context_on_close)(httpServerContext, [](auto *s) {
                Data *appData = (Data *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)(static_dispatch(us_ssl_socket_get_context, us_socket_get_context)(s));

                ((HTTP_SOCKET_DATA_TYPE *) static_dispatch(us_ssl_socket_ext, us_socket_ext)(s))->~HTTP_SOCKET_DATA_TYPE();

                if (appData->onHttpDisconnection) {
                    appData->onHttpDisconnection((HttpSocket<SSL> *) s);
                }
            });

            static_dispatch(us_ssl_socket_context_on_data, us_socket_context_on_data)(httpServerContext, [](auto *s, char *data, int length) {
                Data *appData = (Data *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)(static_dispatch(us_ssl_socket_get_context, us_socket_get_context)(s));

                // warning: should NOT reset timer on any data, ONLY reset data on full HTTP requests!
                // warning: if we are in shutdown state, resetting the timer is a security issue!
                static_dispatch(us_ssl_socket_timeout, us_socket_timeout)(s, HTTP_IDLE_TIMEOUT_S);

                // onHttpRequest should probably be hard-coded to HttpRouter
                ((HttpSocket<SSL> *) s)->onData(data, length, appData->onHttpRequest);

                // compared to routing directly
                //typename Data::UserData user = {(HttpSocket<SSL> *) s, nullptr};
                //appData->r.route("GET", 3, "/", 1, &user);
            });

            static_dispatch(us_ssl_socket_context_on_writable, us_socket_context_on_writable)(httpServerContext, [](auto *s) {

                // what if the client

                // I think it's fair to never mind this one -> if we keep writing data after shutting down then that's an issue for us
                static_dispatch(us_ssl_socket_timeout, us_socket_timeout)(s, HTTP_IDLE_TIMEOUT_S);

                ((HttpSocket<SSL> *) s)->onWritable();
            });

            static_dispatch(us_ssl_socket_context_on_end, us_socket_context_on_end)(httpServerContext, [](auto *s) {
                std::cout << "Socket was half-closed!" << std::endl;
            });

            static_dispatch(us_ssl_socket_context_on_timeout, us_socket_context_on_timeout)(httpServerContext, [](auto *s) {

                if (static_dispatch(us_ssl_socket_is_shut_down, us_socket_is_shut_down)(s)) {
                    std::cout << "Forcefully closing socket since shutdown was not answered in time" << std::endl;
                    static_dispatch(us_ssl_socket_close, us_socket_close)(s);
                } else {
                    std::cout << "Shutting down socket now" << std::endl;
                    static_dispatch(us_ssl_socket_timeout, us_socket_timeout)(s, HTTP_IDLE_TIMEOUT_S);
                    static_dispatch(us_ssl_socket_shutdown, us_socket_shutdown)(s);
                }

            });
        }

public:

    // for server
    void listen(const char *host, int port, int options) {
        static_dispatch(us_ssl_socket_context_listen, us_socket_context_listen)(httpServerContext, host, port, options, sizeof(typename HttpSocket<SSL>::Data));
    }

    AppBase &onPost(std::string pattern, std::function<void(HttpSocket<SSL> *, HttpRequest *, std::vector<std::string_view> *)> handler) {
        data->r.add("POST", pattern.c_str(), [handler](typename Data::UserData *user, auto *args) {
            handler(user->httpSocket, user->httpRequest, args);
        });

        return *this;
    }

    AppBase &onGet(std::string pattern, std::function<void(HttpSocket<SSL> *, HttpRequest *, std::vector<std::string_view> *)> handler) {
        data->r.add("GET", pattern.c_str(), [handler](typename Data::UserData *user, auto *args) {
            handler(user->httpSocket, user->httpRequest, args);
        });

        return *this;
    }

    // why even bother with these?
    AppBase &onHttpConnection(std::function<void(HttpSocket<SSL> *)> handler) {
        data->onHttpConnection = handler;

        return *this;
    }

    AppBase &onHttpDisconnection(std::function<void(HttpSocket<SSL> *)> handler) {
        data->onHttpDisconnection = handler;

        return *this;
    }

    // for client and server
    AppBase &onWebSocket(std::string pattern, std::function<void()> handler) {
        return *this;
    }

    ~AppBase() {
    }
};

class App : public AppBase<false> {

public:
    App() {
        httpServerContext = us_create_socket_context(defaultLoop.loop, sizeof(Data));
        init(defaultLoop.loop);
    }
};

class SSLOptions {
public:
    us_ssl_socket_context_options options = {};

    SSLOptions() {

    }

    SSLOptions &keyFileName(std::string fileName) {
        options.key_file_name = fileName.c_str();
        return *this;
    }

    SSLOptions &certFileName(std::string fileName) {
        options.cert_file_name = fileName.c_str();
        return *this;
    }

    SSLOptions &passphrase(std::string passphrase) {
        options.passphrase = passphrase.c_str();
        return *this;
    }
};

class SSLApp : public AppBase<true> {

public:
    SSLApp(SSLOptions &sslOptions) {
        httpServerContext = us_create_ssl_socket_context(defaultLoop.loop, sizeof(Data), sslOptions.options);
        init(defaultLoop.loop);
    }

};

}

#endif // CONTEXT_H
