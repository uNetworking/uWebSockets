#ifndef HUB_H
#define HUB_H

#include "libusockets.h"
#include <functional>
#include <new>

#include "Http.h"

struct Hub {
    us_loop *loop;
    struct Data {
        Data(us_loop *loop) : httpContext(loop) {

        }

        HttpContext httpContext;
        std::function<void(HttpRequest *, HttpResponse *, char *data, unsigned int length)> onHttpRequest;
    } *data;

    static void wakeupCb(us_loop *loop);
    void onHttpRequest(decltype(Data::onHttpRequest) handler) {
        data->onHttpRequest = handler;
    }

    Hub();
    void listen(const char *host, int port, int options);
    void run();
    ~Hub();
};

#endif // HUB_H
