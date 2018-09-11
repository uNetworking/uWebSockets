#ifndef HUB_H
#define HUB_H

// this header needs fixing! should not depend on source files!

#include "new_design/LoopData.h"

#include <libusockets.h>

// Loop is a bit different, it holds the loop pointer, not as "this"

namespace uWS {
struct Loop {
    us_loop *loop;

    static void wakeupCb(us_loop *loop) {

    }

    static void preCb(us_loop *loop) {

    }

    static void postCb(us_loop *loop) {

    }

    Loop() : loop(us_create_loop(1, wakeupCb, preCb, postCb, sizeof(LoopData))) {

        new (us_loop_ext(loop)) LoopData();
    }

    void run() {
        us_loop_run(loop);
    }

    ~Loop() {
        // deconstruct the loopdata
    }
};

// dessa måste ligga i en sourcefil
thread_local Loop defaultLoop;

static void run() {
    defaultLoop.run();
}
}

#endif // HUB_H
