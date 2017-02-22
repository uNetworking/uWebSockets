#ifndef EPOLL_H
#define EPOLL_H

#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>

// these should be renamed (don't add more than these, only these are used)
typedef int uv_os_sock_t;
typedef void uv_handle_t;
typedef void (*uv_close_cb)(uv_handle_t *);
static const int UV_READABLE = EPOLLIN | EPOLLHUP;
static const int UV_WRITABLE = EPOLLOUT;
static const int UV_VERSION_MINOR = 5;

struct Loop;
struct Poll;
struct Timer;

extern Loop *loops[128];
extern int loopHead;

extern void (*callbacks[128])(Poll *, int, int);
extern int cbHead;

#include <iostream>
#include <chrono>
#include <algorithm>
#include <vector>

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
    epoll_event readyEvents[1024];
    std::chrono::system_clock::time_point timepoint;
    std::vector<Timepoint> timers;

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
    }

    void run();

    int getEpollFd() {
        return epfd;
    }
};

struct Async {
    Async(Loop *loop) {
        std::terminate();
    }

    void start(void (*cb)(Async *)) {

    }

    void send() {

    }

    void close(uv_close_cb cb) {

    }

    void setData(void *data) {

    }

    void *getData() {
        return nullptr;
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
    }

    void setData(void *data) {
        this->data = data;
    }

    void *getData() {
        return data;
    }

    void stop() {
        auto pos = loop->timers.begin();
        for (Timepoint &t : loop->timers) {
            if (t.timer == this) {
                loop->timers.erase(pos);
                break;
            }
            pos++;
        }

        // cannot start a timer again
        loop = nullptr;
    }

    void close(uv_close_cb cb) {
        //std::cout << "Timer::close" << std::endl;
    }
};

// 32 bytes
struct Poll {
    epoll_event event; // 8 bytes
    void *data; // 8 bytes
    int fd = -1; // 4 bytes
    unsigned char loopIndex, cbIndex; // 2 bytes (leaves 2 bytes padding)

    Poll(Loop *loop, uv_os_sock_t fd) {
        init(loop, fd);
    }

    void init(Loop *loop, uv_os_sock_t fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags == -1) {
            std::cout << "Poll::init failure" << std::endl;
            return;// -1;
        }
        flags |= O_NONBLOCK;
        flags = fcntl (fd, F_SETFL, flags);
        if (flags == -1) {
            std::cout << "Poll::init failure" << std::endl;
            return;// -1;
        }

        loopIndex = loop->index;
        this->fd = fd;
        event.events = 0;
        event.data.ptr = this;
        loop->numPolls++;
    }

    Poll() {
        std::cout << "Poll::Poll()" << std::endl;
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
            std::cout << "Poll::setCb increases cbHead to " << cbHead << std::endl;
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

    // all callbacks only hold deletes
    void close(uv_close_cb cb) {
        fd = -1;
        loops[loopIndex]->numPolls--;
    }

    void (*getPollCb())(Poll *, int, int) {
        return (void (*)(Poll *, int, int)) callbacks[cbIndex];
    }

    Loop *getLoop() {
        return loops[loopIndex];
    }
};

#endif // EPOLL_H
