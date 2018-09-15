#ifndef LOOP_H
#define LOOP_H

/* The loop is lazily created per-thread and run with uWS::run() */

#include "LoopData.h"

#include <libusockets.h>

#include <thread>

namespace uWS {
struct Loop {
private:
    static void wakeupCb(us_loop *loop) {

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
            std::cout << "Created default loop " << defaultLoop << " for thread " << std::this_thread::get_id() << std::endl;
            return defaultLoop;
        } else if (ownsDefaultLoop) {
            std::cout << "Returned default loop " << defaultLoop << " for thread " << std::this_thread::get_id() << std::endl;
            return defaultLoop;
        }

        /* Other threads get their non-default loops lazily created */
        static thread_local Loop *threadLocalLoop;
        if (!threadLocalLoop) {
            threadLocalLoop = create(false);
            std::cout << "Created non-default loop " << threadLocalLoop << " for thread " << std::this_thread::get_id() << std::endl;
            return threadLocalLoop;
        }
        std::cout << "Returned non-default loop " << threadLocalLoop << " for thread " << std::this_thread::get_id() << std::endl;
        return threadLocalLoop;
    }

    /* Freeing the default loop should be done once */
    void free() {
        us_loop_free((us_loop *) this);
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
