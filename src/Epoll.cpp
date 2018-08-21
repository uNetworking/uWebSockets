#include "Backend.h"

#ifdef USE_EPOLL

namespace uS {

// todo: remove this mutex, have callbacks set at program start
std::recursive_mutex cbMutex;
void (*callbacks[16])(Poll *, int, int);
int cbHead = 0;

void Loop::doEpoll(int epollTimeout) {
    for (std::pair<Poll *, void (*)(Poll *)> c : closing) {
        numPolls--;

        c.second(c.first);

        if (!numPolls) {
            closing.clear();
            return;
        }
    }
    closing.clear();

    int numFdReady = epoll_wait(epfd, readyEvents, 1024, epollTimeout);
    timepoint = std::chrono::system_clock::now();

    if (preCb) {
        preCb(preCbData);
    }

    for (int i = 0; i < numFdReady; i++) {
        Poll *poll = (Poll *) readyEvents[i].data.ptr;
        int status = -bool(readyEvents[i].events & EPOLLERR);
        callbacks[poll->state.cbIndex](poll, status, readyEvents[i].events);
    }

    if (timers.size()) {
        if (timers[0].timepoint < timepoint) {
            do {
                Timer *timer = timers[0].timer;
                processingTimer = timer; // processing this timer
                cancelledLastTimer = false;
                timers[0].cb(timers[0].timer);

                if (cancelledLastTimer) {
                    continue;
                }

                int repeat = timers[0].nextDelay;
                auto cb = timers[0].cb;
                timers.erase(timers.begin());
                if (repeat) {
                    timer->start(cb, repeat, repeat);
                }
            } while (timers.size() && timers[0].timepoint < timepoint);

        } else { // we have a timer but it did not process, so update our next delay
            delay = std::max<int>(std::chrono::duration_cast<std::chrono::milliseconds>(timers[0].timepoint - timepoint).count(), 0);
        }
    }

    if (postCb) {
        postCb(postCbData);
    }
}

void Loop::run() {
    // updated for consistency with libuv impl. behaviour
    timepoint = std::chrono::system_clock::now();
    while (numPolls) {
        doEpoll(delay);
    }
}

void Loop::poll() {
    if (numPolls) {
        doEpoll(0);
    } else {
        // updated for consistency with libuv impl. behaviour
        timepoint = std::chrono::system_clock::now();
    }
}

}

#endif
