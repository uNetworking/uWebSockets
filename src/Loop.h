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

        /* trying this one here */
        for (auto &f : loopData->postHandlers) {
            f((Loop *) loop);
        }
    }

    static void postCb(us_loop *loop) {
        LoopData *loopData = (LoopData *) us_loop_ext(loop);

        /* We should move over to using only these */
        for (auto &f : loopData->postHandlers) {
            f((Loop *) loop);
        }

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

    /* Todo: should take void ptr */
    static Loop *create(bool defaultLoop) {
        return ((Loop *) us_create_loop(defaultLoop, wakeupCb, preCb, postCb, sizeof(LoopData)))->init();
    }

public:
    /* Lazily initializes a per-thread loop and returns it.
     * Will automatically free all initialized loops at exit. */
    static Loop *get(void *existingNativeLoop = nullptr) {
        static thread_local Loop *lazyLoop;
        if (!lazyLoop) {
            /* If we are given a native loop pointer we pass that to uSockets and let it deal with it */
            if (existingNativeLoop) {
                /* Todo: here we want to pass the pointer, not a boolean */
                lazyLoop = create(true);
                /* We cannot register automatic free here, must be manually done */
            } else {
                lazyLoop = create(false);
                std::atexit([]() {
                    Loop::get()->free();
                });
            }
        }

        return lazyLoop;
    }

    /* Freeing the default loop should be done once */
    void free() {
        LoopData *loopData = (LoopData *) us_loop_ext((us_loop *) this);
        loopData->~LoopData();
        /* uSockets will track whether this loop is owned by us or a borrowed alien loop */
        us_loop_free((us_loop *) this);
    }

    /* We want to have multiple of these */
    void addPostHandler(fu2::unique_function<void(Loop *)> &&handler) {
        LoopData *loopData = (LoopData *) us_loop_ext((us_loop *) this);

        loopData->postHandlers.emplace_back(std::move(handler));
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
    Loop::get()->run();
}

}

#endif // LOOP_H
