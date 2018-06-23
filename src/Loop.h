// should lie in hub and be connected with pre/post callbacks
char *corkBuffer = new char[1024];
int corkOffset = 0;

#ifndef HUB_H
#define HUB_H

#include "libusockets.h"
#include <functional>
#include <new>
#include <string_view>
#include <iostream>

namespace uWS {
struct Loop {
    us_loop *loop;

    struct Data {
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
