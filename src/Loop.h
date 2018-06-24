#ifndef HUB_H
#define HUB_H

#include "libusockets.h"
#include <functional>
#include <new>
#include <string_view>
#include <iostream>
#include <unistd.h>

namespace uWS {
struct Loop {
    us_loop *loop;

    static const int CORK_BUFFER_SIZE = 16 * 1024;
    static const int MAX_COPY_DISTANCE = 4 * 1024;

    struct Data {

        char *corkBuffer = new char[CORK_BUFFER_SIZE];
        int corkOffset = 0;

        Data() {

        }

    } *data;

    static void wakeupCb(us_loop *loop) {

    }

    static void preCb(us_loop *loop) {

    }

    static void postCb(us_loop *loop) {

    }

    Loop() : loop(us_create_loop(wakeupCb, preCb, postCb, sizeof(Data))) {
        new (data = (Data *) us_loop_ext(loop)) Data();
    }

    void run() {
        us_loop_run(loop);
    }

    ~Loop() {

    }
};

// dessa måste ligga i en sourcefil
thread_local Loop defaultLoop;

static void run() {
    defaultLoop.run();
}
}

#endif // HUB_H
