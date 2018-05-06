#ifndef HUB_H
#define HUB_H

#include "libusockets.h"
#include <functional>
#include <new>

#include "Http.h"

struct Hub {
    us_loop *loop;
    struct Data {
        Data() {

        }

        HttpContext *httpContext;
        std::function<void(HttpSocket *)> onHttpConnection;
        std::function<void(HttpSocket *)> onHttpDisconnection;
        std::function<void(HttpSocket *, HttpRequest *, char *data, unsigned int length)> onHttpRequest;
    } *data;

    static void wakeupCb(us_loop *loop);
    void onHttpConnection(decltype(Data::onHttpConnection) handler) {
        data->onHttpConnection = handler;
    }
    void onHttpDisconnection(decltype(Data::onHttpDisconnection) handler) {
        data->onHttpDisconnection = handler;
    }
    void onHttpRequest(decltype(Data::onHttpRequest) handler) {
        data->onHttpRequest = handler;
    }

    Hub();
    void listen(const char *host, int port, int options);
    void run();
    ~Hub();
};

#endif // HUB_H
