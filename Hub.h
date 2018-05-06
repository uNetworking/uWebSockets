#ifndef HUB_H
#define HUB_H

#include "libusockets.h"
#include <functional>
#include <new>

#include "Http.h"

struct Hub {
    us_loop *loop;

    static void wakeupCb(us_loop *loop) {

    }

    struct Data {
        Data(us_loop *loop) : httpContext(loop) {

        }

        HttpContext httpContext;
        std::function<void(HttpRequest *, HttpResponse *)> onHttpRequest;
    } *data;

    void onHttpRequest(decltype(Data::onHttpRequest) handler) {
        data->onHttpRequest = handler;
    }

    Hub() {
        loop = us_create_loop(wakeupCb, sizeof(Data));
        new (data = (Data *) us_loop_userdata(loop)) Data(loop);
    }

    // should we expose us_listen_socket here?
    // us_listen_socket_close(listen_socket) is easy so why not?
    void listen(const char *host, int port, int options) {
        data->httpContext.listen(host, port, options);
    }

    void run() {
        us_loop_run(loop);
    }

    ~Hub() {
        // us_loop_delete(loop);
    }
};

#endif // HUB_H
