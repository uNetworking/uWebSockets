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

struct Loop;
struct Poll;
struct Timer;

extern Loop *loops[128];
extern void (*callbacks[128])(Poll *, int, int);
extern int loopHead, cbHead;

struct Timepoint {
    void (*cb)(Timer *);
    Timer *timer;
    std::chrono::system_clock::time_point timepoint;
    int nextDelay;
};

struct Loop {
    int epfd;
    unsigned char index;
    int numPolls = 0;
    bool cancelledLastTimer;
    int delay = -1;
    epoll_event readyEvents[1024];
    std::chrono::system_clock::time_point timepoint;
    std::vector<Timepoint> timers;
    std::vector<Poll *> closing;

    Loop(bool defaultLoop) {
        epfd = epoll_create(1);
        loops[index = loopHead++] = this;
        timepoint = std::chrono::system_clock::now();
    }

    static Loop *createLoop(bool defaultLoop = true) {
        return new Loop(defaultLoop);
    }

    void destroy() {
        ::close(epfd);
        // todo: proper removal
        loopHead--;

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

// 32 bytes
struct Poll {
    epoll_event event; // 8 bytes
    void *data; // 8 bytes
    int fd = -1; // 4 bytes
    unsigned char loopIndex, cbIndex; // 2 bytes (leaves 2 bytes padding)

    // up to 4k loops
    /*struct {
        int cbIndex : 4;
        int loopIndex : 12;
    };*/

    Poll(Loop *loop, uv_os_sock_t fd) {
        init(loop, fd);
    }

    void init(Loop *loop, uv_os_sock_t fd) {
        fcntl (fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
        loopIndex = loop->index;
        this->fd = fd;
        event.events = 0;
        event.data.ptr = this;
        loop->numPolls++;
    }

    // todo: remove these, fix up connection callback
    Poll() {

    }

    ~Poll() {
    }

    void setData(void *data) {
        this->data = data;
    }

    bool isClosing() {
        return fd == -1;
    }

    uv_os_sock_t getFd() {
        return fd;
    }

    void *getData() {
        return data;
    }

    void setCb(void (*cb)(Poll *p, int status, int events)) {
        cbIndex = cbHead;
        for (int i = 0; i < cbHead; i++) {
            if (callbacks[i] == cb) {
                cbIndex = i;
                break;
            }
        }
        if (cbIndex == cbHead) {
            callbacks[cbHead++] = cb;
        }
    }

    void start(int events) {
        event.events = events;
        epoll_ctl(loops[loopIndex]->epfd, EPOLL_CTL_ADD, fd, &event);
    }

    void change(int events) {
        event.events = events;
        epoll_ctl(loops[loopIndex]->epfd, EPOLL_CTL_MOD, fd, &event);
    }

    void stop() {
        epoll_ctl(loops[loopIndex]->epfd, EPOLL_CTL_DEL, fd, &event);
    }

    void close() {
        fd = -1;
        loops[loopIndex]->closing.push_back(this);
    }

    void (*getPollCb())(Poll *, int, int) {
        return (void (*)(Poll *, int, int)) callbacks[cbIndex];
    }

    Loop *getLoop() {
        return loops[loopIndex];
    }
};

struct Async : Poll {
    void (*cb)(Async *);

    Async(Loop *loop) : Poll(loop, ::eventfd(0, 0)) {
    }

    void start(void (*cb)(Async *)) {
        this->cb = cb;
        Poll::setCb([](Poll *p, int, int) {
            uint64_t val;
            if (::read(p->fd, &val, 8) == 8) {
                ((Async *) p)->cb((Async *) p);
            }
        });
        Poll::start(UV_READABLE);
    }

    void send() {
        uint64_t one = 1;
        ::write(fd, &one, 8);
    }

    void close() {
        Poll::stop();
        ::close(fd);
        Poll::close();
    }
};

#endif // EPOLL_H
