#ifndef UUV_H
#define UUV_H

#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <arpa/inet.h>

#define UV_VERSION_MINOR 3

#include <vector>
#include <chrono>
#include <algorithm>

struct uv_handle_t {
    unsigned char type = 0;
};

struct uv_async_t : uv_handle_t {
    void *data;
};

struct uv_loop_t;

struct uv_timer_t : uv_handle_t {
    void *data;
    uv_loop_t *loop;
};

static void uv_async_send(uv_async_t *async) {
    // todo
}

struct uv_poll_t;
struct uv_handle_t;

static const int UV_WRITABLE = EPOLLOUT;
static const int UV_READABLE = EPOLLIN | EPOLLHUP;
static const int UV_RUN_DEFAULT = 0;
typedef int uv_os_sock_t;
typedef void (*uv_poll_cb)(uv_poll_t *poll, int status, int events);
typedef void (*uv_handle_cb)(uv_handle_t *handle);
typedef void (*uv_async_cb)(uv_async_t *handle);
typedef void (*uv_timer_cb)(uv_timer_t *handle);

struct Timepoint {
    uv_timer_cb cb;
    uv_timer_t *timer;
    std::chrono::system_clock::time_point timepoint;
    int nextDelay;
};

struct uv_loop_t {
    int efd;
    int index;
    std::vector<std::pair<uv_handle_t *, uv_handle_cb>> closing;
    int polls;
    std::vector<Timepoint> timers;
    std::chrono::system_clock::time_point timepoint;
};

static void uv_timer_init(uv_loop_t *loop, uv_timer_t *timer) {
    timer->type = 1;
    timer->loop = loop;
    loop->timepoint = std::chrono::system_clock::now();
}

// optimera
static void uv_timer_start(uv_timer_t *timer, uv_timer_cb cb, int timeout, int repeat) {
    std::chrono::system_clock::time_point timepoint = timer->loop->timepoint + std::chrono::milliseconds(timeout);

    // sortera!
    timer->loop->timers.push_back({cb, timer, timepoint, repeat});

    std::sort(timer->loop->timers.begin(), timer->loop->timers.end(), [](const Timepoint &a, const Timepoint &b) {
        return a.timepoint < b.timepoint;
    });
}

static void uv_timer_stop(uv_timer_t *timer) {
    auto pos = timer->loop->timers.begin();
    for (Timepoint &t : timer->loop->timers) {
        if (t.timer == timer) {
            timer->loop->timers.erase(pos);
            break;
        }
        pos++;
    }
}

static void uv_async_init(uv_loop_t *loop, uv_async_t *async, uv_async_cb cb) {
    async->type = 2;
}

static uv_loop_t *uv_default_loop() {
    return nullptr;
}

// remove these when done with them!
extern uv_loop_t *loops[128];
extern int loopHead;

extern uv_poll_cb callbacks[128];
extern int cbHead;

struct uv_poll_t : uv_handle_t {
    epoll_event event;
    void *data;
    int fd;
    unsigned char loopIndex;
    unsigned char cbIndex;

    uv_poll_cb get_poll_cb() {
        return callbacks[cbIndex];
    }

    uv_loop_t *get_loop() {
        return loops[loopIndex];
    }
};

inline int uv_poll_init_socket(uv_loop_t *loop, uv_poll_t *poll, uv_os_sock_t socket) {
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }
    flags |= O_NONBLOCK;
    flags = fcntl (socket, F_SETFL, flags);
    if (flags == -1) {
        return -1;
    }

    poll->loopIndex = loop->index;
    poll->fd = socket;
    poll->event.events = 0;
    poll->event.data.ptr = poll;
    loop->polls++;
    return epoll_ctl(loop->efd, EPOLL_CTL_ADD, socket, &poll->event);
}

inline int uv_poll_start(uv_poll_t *poll, int events, uv_poll_cb cb) {
    poll->event.events = events;
    poll->cbIndex = cbHead;
    for (int i = 0; i < cbHead; i++) {
        if (callbacks[i] == cb) {
            poll->cbIndex = i;
            break;
        }
    }
    if (poll->cbIndex == cbHead) {
        callbacks[cbHead++] = cb;
    }
    return epoll_ctl(loops[poll->loopIndex]->efd, EPOLL_CTL_MOD, poll->fd, &poll->event);
}

inline int uv_poll_stop(uv_poll_t *poll) {
    return epoll_ctl(loops[poll->loopIndex]->efd, EPOLL_CTL_DEL, poll->fd, &poll->event);
}

inline uv_loop_t *uv_loop_new() {
    uv_loop_t *loop = new uv_loop_t;
    loop->efd = epoll_create(1);
    loop->index = loopHead++;
    loop->polls = 0;
    loops[loop->index] = loop;
    return loop;
}

inline void uv_loop_delete(uv_loop_t *loop) {
    close(loop->efd);
    delete loop;
}

inline void uv_close(uv_handle_t *handle, uv_handle_cb cb) {
    if (handle->type == 0) {
        uv_poll_t *poll = (uv_poll_t *) handle;
        poll->fd = -1;
        poll->get_loop()->closing.push_back({handle, cb});
    }
}

inline bool uv_is_closing(uv_handle_t *handle) {
    return ((uv_poll_t *) handle)->fd == -1;
}

inline void uv_run(uv_loop_t *loop, int mode) {
    loop->timepoint = std::chrono::system_clock::now();
    while (loop->polls) {
        for (std::pair<uv_handle_t *, uv_handle_cb> c : loop->closing) {
            loop->polls--;
            c.second(c.first);

            if (!loop->polls) {
                loop->closing.clear();
                return;
            }
        }
        loop->closing.clear();

        int delay = -1;
        if (loop->timers.size()) {
            delay = std::max<int>(std::chrono::duration_cast<std::chrono::milliseconds>(loop->timers[0].timepoint - loop->timepoint).count(), 0);
        }

        epoll_event readyEvents[64];
        int numFdReady = epoll_wait(loop->efd, readyEvents, 64, delay);
        for (int i = 0; i < numFdReady; i++) {
            uv_poll_t *poll = (uv_poll_t *) readyEvents[i].data.ptr;
            int status = -bool(readyEvents[i].events & EPOLLERR);
            callbacks[poll->cbIndex](poll, status, readyEvents[i].events);
        }

        loop->timepoint = std::chrono::system_clock::now();
        while (loop->timers.size() && loop->timers[0].timepoint < loop->timepoint) {
            loop->timers[0].cb(loop->timers[0].timer);
            uv_timer_t *timer = loop->timers[0].timer;
            int repeat = loop->timers[0].nextDelay;
            uv_timer_cb cb = loop->timers[0].cb;
            loop->timers.erase(loop->timers.begin());
            if (repeat) {
                uv_timer_start(timer, cb, repeat, repeat);
            }
        }
    }
}

inline int uv_fileno(uv_handle_t *handle) {
    return ((uv_poll_t *) handle)->fd;
}

#endif // UUV_H
