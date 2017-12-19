#include "Backend.h"

#ifdef USE_EPOLL

namespace uS {

// todo: remove this mutex, have callbacks set at program start
std::recursive_mutex cbMutex;
void (*callbacks[16])(Poll *, int, int);
int cbHead = 0;

void Loop::run() {
    timepoint = std::chrono::system_clock::now();
    while (numPolls) {
        for (std::pair<Poll *, void (*)(Poll *)> c : closing) {
            numPolls--;

            c.second(c.first);

            if (!numPolls) {
                closing.clear();
                return;
            }
        }
        closing.clear();

        int numFdReady = epoll_wait(epfd, readyEvents, 1024, delay);
        timepoint = std::chrono::system_clock::now();

        if (preCb) {
            preCb(preCbData);
        }

        for (int i = 0; i < numFdReady; i++) {
            Poll *poll = (Poll *) readyEvents[i].data.ptr;
            int status = -bool(readyEvents[i].events & EPOLLERR);
            callbacks[poll->state.cbIndex](poll, status, readyEvents[i].events);
        }

        while (timers.size() && timers[0].timepoint < timepoint) {
            Timer *timer = timers[0].timer;
            cancelledLastTimer = false;
            timers[0].cb(timers[0].timer);

            if (cancelledLastTimer) {
                continue;
            }

            int repeat = timers[0].nextDelay;
            int timeout = timers[0].timeout;
            auto cb = timers[0].cb;
            timers.erase(timers.begin());
            if (repeat) {
                timer->start(cb, timeout, repeat);
            }
        }

        if (postCb) {
            postCb(postCbData);
        }
    }
}

}

#endif
