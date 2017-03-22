#include "Backend.h"

#ifdef USE_EPOLL
Loop *loops[128];
int loopHead = 0;

void (*callbacks[128])(Poll *, int, int);
int cbHead = 0;

void Loop::run() {
    timepoint = std::chrono::system_clock::now();
    while (numPolls) {
        for (Poll *c : closing) {
            numPolls--;

            // probably not correct
            delete c;

            if (!numPolls) {
                closing.clear();
                return;
            }
        }
        closing.clear();

        int numFdReady = epoll_wait(epfd, readyEvents, 1024, delay);
        for (int i = 0; i < numFdReady; i++) {
            Poll *poll = (Poll *) readyEvents[i].data.ptr;
            int status = -bool(readyEvents[i].events & EPOLLERR);
            callbacks[poll->cbIndex](poll, status, readyEvents[i].events);
        }

        timepoint = std::chrono::system_clock::now();
        while (timers.size() && timers[0].timepoint < timepoint) {
            Timer *timer = timers[0].timer;
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
        }
    }
}
#endif
