#ifndef CONTEXT_H
#define CONTEXT_H

#include "libusockets.h"
#include <type_traits>
#include "Loop.h"
#include "Http.h"
#include "HttpRouter.h"

namespace uWS {

template <bool SSL>
class AppBase {

protected:

    template <class A, class B>
    static constexpr typename std::conditional<SSL, A, B>::type *static_dispatch(A *a, B *b) {
        if constexpr(SSL) {
            return a;
        } else {
            return b;
        }
    }

    typedef typename std::conditional<SSL, us_ssl_socket_context, us_socket_context>::type SOCKET_CONTEXT_TYPE;

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
                Data *data = (Data *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)(static_dispatch(us_ssl_socket_get_context, us_socket_get_context)(s));

                // here we need to construct a HTTP socket on the ext!

                if (!data->onHttpConnection) {
                    return;
                }

                data->onHttpConnection((HttpSocket<SSL> *) s);
            });

            static_dispatch(us_ssl_socket_context_on_close, us_socket_context_on_close)(httpServerContext, [](auto *s) {
                Data *data = (Data *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)(static_dispatch(us_ssl_socket_get_context, us_socket_get_context)(s));

                if (!data->onHttpDisconnection) {
                    return;
                }

                data->onHttpDisconnection((HttpSocket<SSL> *) s);
            });

            static_dispatch(us_ssl_socket_context_on_data, us_socket_context_on_data)(httpServerContext, [](auto *s, char *data, int length) {

                Data *contextData = (Data *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)(static_dispatch(us_ssl_socket_get_context, us_socket_get_context)(s));

                    // a HttpSocket is basically the Http state and everything needed for it, we use it to parse the data and it knows about its context

                    //

                HttpRequest req(data, length);
                if (req.isComplete()) {
                    contextData->onHttpRequest((HttpSocket<SSL> *) s, &req);
                } else {
                    std::cout << "Got chunked HTTP headers!" << std::endl;
                }
            });
        }

public:

    // for server
    void listen(const char *host, int port, int options) {
        static_dispatch(us_ssl_socket_context_listen, us_socket_context_listen)(httpServerContext, host, port, options, sizeof(HttpSocket<SSL>));
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
