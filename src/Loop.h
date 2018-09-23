#ifndef LOOP_H
#define LOOP_H

/* The loop is lazily created per-thread and run with uWS::run() */

#include "LoopData.h"

#include <libusockets.h>




#include <iostream>

namespace uWS {
struct Loop {
private:
    static void wakeupCb(us_loop *loop) {
        std::cout << "wakeupCB called" << std::endl;
        LoopData *loopData = (LoopData *) us_loop_ext(loop);

        /* Swap current deferQueue */
        loopData->deferMutex.lock();
        int oldDeferQueue = loopData->currentDeferQueue;
        loopData->currentDeferQueue = (loopData->currentDeferQueue + 1) % 2;
        loopData->deferMutex.unlock();

        /* Drain the queue */
        for (auto &x : loopData->deferQueues[oldDeferQueue]) {
            x();
        }
        loopData->deferQueues[oldDeferQueue].clear();
    }

    static void preCb(us_loop *loop) {

    }

    static void postCb(us_loop *loop) {

    }

    Loop() = delete;

    Loop *init() {
        new (us_loop_ext((us_loop *) this)) LoopData();
        return this;
    }

    static Loop *create(bool defaultLoop) {
        return ((Loop *) us_create_loop(defaultLoop, wakeupCb, preCb, postCb, sizeof(LoopData)))->init();
    }

public:
    /* Returns the default loop if called from one thread, or a dedicated per-thread loop if called from multiple threads */
    static Loop *defaultLoop() {
        /* Deliver and attach the default loop to the first thread who calls us */
        static thread_local bool ownsDefaultLoop;
        static Loop *defaultLoop;
        if (!defaultLoop) {
            ownsDefaultLoop = true;
            defaultLoop = create(true);
            return defaultLoop;
        } else if (ownsDefaultLoop) {
            return defaultLoop;
        }

        /* Other threads get their non-default loops lazily created */
        static thread_local Loop *threadLocalLoop;
        if (!threadLocalLoop) {
            threadLocalLoop = create(false);
            return threadLocalLoop;
        }
        return threadLocalLoop;
    }

    /* Freeing the default loop should be done once */
    void free() {
        us_loop_free((us_loop *) this);
    }

    /* Defer this callback on Loop's thread of execution */
    void defer(std::function<void()> cb) {
        LoopData *loopData = (LoopData *) us_loop_ext((us_loop *) this);

        std::cout << "defer called" << std::endl;
        //if (std::thread::get_id() == ) // todo: add fast path for same thread id
        loopData->deferMutex.lock();
        loopData->deferQueues[loopData->currentDeferQueue].emplace_back(cb);
        loopData->deferMutex.unlock();

        std::cout << "us_wakeup_loop called" << std::endl;
        us_wakeup_loop((us_loop *) this);
    }

    /* Actively block and run this loop */
    void run() {
        us_loop_run((us_loop *) this);
    }

    /* Passively integrate with the underlying default loop */
    /* Used to seamlessly integrate with third parties such as Node.js */
    void integrate() {

    }
};

/* Can be called from any thread to run the thread local loop */
void run() {
    Loop::defaultLoop()->run();
}

}

#endif // LOOP_H
