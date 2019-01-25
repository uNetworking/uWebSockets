/*
 * Authored by Alex Hultman, 2018-2019.
 * Intellectual property of third-party.

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 *     http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LOOP_H
#define LOOP_H

/* The loop is lazily created per-thread and run with uWS::run() */

#include "LoopData.h"

#include <libusockets_new.h>




#include <iostream>

namespace uWS {
struct Loop {
private:
    static void wakeupCb(us_loop *loop) {
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
        LoopData *loopData = (LoopData *) us_loop_ext(loop);

        if (loopData->preHandler) {
            loopData->preHandler((Loop *) loop);
        }
    }

    static void postCb(us_loop *loop) {
        LoopData *loopData = (LoopData *) us_loop_ext(loop);

        if (loopData->postHandler) {
            loopData->postHandler((Loop *) loop);
        }
    }

    Loop() = delete;
    ~Loop() = default;

    Loop *init() {
        new (us_loop_ext((us_loop *) this)) LoopData;
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
            std::atexit([]() {
                Loop::defaultLoop()->free();
            });
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
        LoopData *loopData = (LoopData *) us_loop_ext((us_loop *) this);
        loopData->~LoopData();
        us_loop_free((us_loop *) this);
    }

    /* Set postCb callback */
    void setPostHandler(fu2::unique_function<void(Loop *)> &&handler) {
        LoopData *loopData = (LoopData *) us_loop_ext((us_loop *) this);

        loopData->postHandler = std::move(handler);
    }

    void setPreHandler(fu2::unique_function<void(Loop *)> &&handler) {
        LoopData *loopData = (LoopData *) us_loop_ext((us_loop *) this);

        loopData->preHandler = std::move(handler);
    }

    /* Defer this callback on Loop's thread of execution */
    void defer(fu2::unique_function<void()> &&cb) {
        LoopData *loopData = (LoopData *) us_loop_ext((us_loop *) this);

        //if (std::thread::get_id() == ) // todo: add fast path for same thread id
        loopData->deferMutex.lock();
        loopData->deferQueues[loopData->currentDeferQueue].emplace_back(std::move(cb));
        loopData->deferMutex.unlock();

        us_wakeup_loop((us_loop *) this);
    }

    /* Actively block and run this loop */
    void run() {
        us_loop_run((us_loop *) this);
    }

    /* Passively integrate with the underlying default loop */
    /* Used to seamlessly integrate with third parties such as Node.js */
    void integrate() {
        us_loop_integrate((us_loop *) this);
    }
};

/* Can be called from any thread to run the thread local loop */
inline void run() {
    Loop::defaultLoop()->run();
}

}

#endif // LOOP_H
