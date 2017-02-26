#include "Backend.h"

#ifdef USE_EPOLL
Loop *loops[128];
int loopHead = 0;

void (*callbacks[128])(Poll *, int, int);
int cbHead = 0;

void Loop::run() {
    std::cout << "Loop::run" << std::endl;
    timepoint = std::chrono::system_clock::now();
    while (numPolls) {
        for (Poll *c : closing) {
            numPolls--;
            //c.second(c.first);

            // probably not correct
            delete c;

            if (!numPolls) {
                closing.clear();
                return;
            }
        }
        closing.clear();

        // timers should really allow immediate close!
        for (Timer *c : closingTimers) {
            // probably not correct
            delete c;
        }
        closingTimers.clear();

        int delay = -1;
        if (timers.size()) {
            delay = std::max<int>(std::chrono::duration_cast<std::chrono::milliseconds>(timers[0].timepoint - timepoint).count(), 0);
        }

        int numFdReady = epoll_wait(epfd, readyEvents, 1024, delay);
        for (int i = 0; i < numFdReady; i++) {
            Poll *poll = (Poll *) readyEvents[i].data.ptr;
            int status = -bool(readyEvents[i].events & EPOLLERR);
            callbacks[poll->cbIndex](poll, status, readyEvents[i].events);
        }

        timepoint = std::chrono::system_clock::now();
        while (timers.size() && timers[0].timepoint < timepoint) {
            Timer *timer = timers[0].timer;
            timers[0].cb(timers[0].timer);

            // stoppades den?
            if (!timer->loop) {
                continue;
            }

            int repeat = timers[0].nextDelay;
            auto cb = timers[0].cb;
            timers.erase(timers.begin());
            if (repeat) {
                std::cout << "Repeating timer now!" << std::endl;
                timer->start(cb, repeat, repeat);
            }
        }
    }
    std::cout << "Loop::run falling through" << std::endl;
}
#endif
