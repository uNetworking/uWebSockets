#ifndef EPOLL_H
#define EPOLL_H

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <chrono>
#include <algorithm>
#include <vector>

typedef int uv_os_sock_t;
static const int UV_READABLE = EPOLLIN;
static const int UV_WRITABLE = EPOLLOUT;

struct Poll;
struct Timer;

extern void (*callbacks[16])(Poll *, int, int);
extern int cbHead;

struct Timepoint {
    void (*cb)(Timer *);
    Timer *timer;
    std::chrono::system_clock::time_point timepoint;
    int nextDelay;
};

struct Loop {
    int epfd;
    int numPolls = 0;
    bool cancelledLastTimer;
    int delay = -1;
    epoll_event readyEvents[1024];
    std::chrono::system_clock::time_point timepoint;
    std::vector<Timepoint> timers;
    std::vector<Poll *> closing;

    Loop(bool defaultLoop) {
        epfd = epoll_create(1);
        timepoint = std::chrono::system_clock::now();
    }

    static Loop *createLoop(bool defaultLoop = true) {
        return new Loop(defaultLoop);
    }

    void destroy() {
        ::close(epfd);
        delete this;
    }

    void run();

    int getEpollFd() {
        return epfd;
    }
};

struct Timer {
    Loop *loop;
    void *data;

    Timer(Loop *loop) {
        this->loop = loop;
    }

    void start(void (*cb)(Timer *), int timeout, int repeat) {
        std::chrono::system_clock::time_point timepoint = loop->timepoint + std::chrono::milliseconds(timeout);
        loop->timers.push_back({cb, this, timepoint, repeat});
        std::sort(loop->timers.begin(), loop->timers.end(), [](const Timepoint &a, const Timepoint &b) {
            return a.timepoint < b.timepoint;
        });

        // insertion sort


        loop->delay = -1;
        if (loop->timers.size()) {
            loop->delay = std::max<int>(std::chrono::duration_cast<std::chrono::milliseconds>(loop->timers[0].timepoint - loop->timepoint).count(), 0);
        }
    }

    void setData(void *data) {
        this->data = data;
    }

    void *getData() {
        return data;
    }

    // always called before destructor
    void stop() {
        auto pos = loop->timers.begin();
        for (Timepoint &t : loop->timers) {
            if (t.timer == this) {
                loop->timers.erase(pos);
                break;
            }
            pos++;
        }
        loop->cancelledLastTimer = true;

        loop->delay = -1;
        if (loop->timers.size()) {
            loop->delay = std::max<int>(std::chrono::duration_cast<std::chrono::milliseconds>(loop->timers[0].timepoint - loop->timepoint).count(), 0);
        }
    }

    void close() {
        delete this;
    }
};

// 4 bytes
struct Poll {
    struct {
        int fd : 28;
        unsigned int cbIndex : 4;
    } state = {-1, 0};

    Poll(Loop *loop, uv_os_sock_t fd) {
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
        state.fd = fd;
        loop->numPolls++;
    }

    bool isClosing() {
        return state.fd == -1;
    }

    uv_os_sock_t getFd() {
        return state.fd;
    }

    void setCb(void (*cb)(Poll *p, int status, int events)) {
        state.cbIndex = cbHead;
        for (int i = 0; i < cbHead; i++) {
            if (callbacks[i] == cb) {
                state.cbIndex = i;
                break;
            }
        }
        if (state.cbIndex == cbHead) {
            callbacks[cbHead++] = cb;
        }
    }

    // what you put in self is what comes in setCb
    void start(Loop *loop, Poll *self, int events) {
        epoll_event event;
        event.events = events;
        event.data.ptr = self;
        epoll_ctl(loop->epfd, EPOLL_CTL_ADD, state.fd, &event);
    }

    void change(Loop *loop, Poll *self, int events) {
        epoll_event event;
        event.events = events;
        event.data.ptr = self;
        epoll_ctl(loop->epfd, EPOLL_CTL_MOD, state.fd, &event);
    }

    void stop(Loop *loop) {
        epoll_event event;
        epoll_ctl(loop->epfd, EPOLL_CTL_DEL, state.fd, &event);
    }

    void close(Loop *loop) {
        state.fd = -1;
        loop->closing.push_back(this);
    }
};

struct Async : Poll {
    void (*cb)(Async *);
    Loop *loop;
    void *data;

    Async(Loop *loop) : Poll(loop, ::eventfd(0, 0)) {
        this->loop = loop;
    }

    void start(void (*cb)(Async *)) {
        this->cb = cb;
        Poll::setCb([](Poll *p, int, int) {
            uint64_t val;
            if (::read(p->state.fd, &val, 8) == 8) {
                ((Async *) p)->cb((Async *) p);
            }
        });
        Poll::start(loop, this, UV_READABLE);
    }

    void send() {
        uint64_t one = 1;
        ::write(state.fd, &one, 8);
    }

    void close() {
        Poll::stop(loop);
        ::close(state.fd);
        Poll::close(loop);
    }

    void setData(void *data) {
        this->data = data;
    }

    void *getData() {
        return data;
    }
};

#endif // EPOLL_H
