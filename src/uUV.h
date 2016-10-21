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

// placeholders here

struct uv_async_t {
    void *data;
};

struct uv_timer_t {
    void *data;
};

#define uv_async_init(x, y, z)
#define uv_async_send(x)
#define uv_timer_init(x, y)
#define uv_timer_start(x, y, z, w)
#define uv_timer_stop(x)


struct uv_poll_t;
struct uv_handle_t;

static const int UV_WRITABLE = EPOLLOUT;
static const int UV_READABLE = EPOLLIN | EPOLLHUP;
static const int UV_RUN_DEFAULT = 0;
typedef int uv_os_sock_t;
typedef void (*uv_poll_cb)(uv_poll_t *poll, int status, int events);
typedef void (*uv_handle_cb)(uv_handle_t *handle);

struct uv_loop_t {
    int efd;
    int index;
};

static uv_loop_t *uv_default_loop() {
    return nullptr;
}

struct uv_handle_t {

};

// support 128 loops maximum
extern uv_loop_t *loops[128];
extern int loopHead;

// support 128 callbacks maximum
extern uv_poll_cb callbacks[128];
extern int cbHead;

// 24 byte poll
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
    // mark socket nonblocking
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
    //poll->event = new epoll_event;
    poll->fd = socket;
    poll->event.events = 0;
    poll->event.data.ptr = poll;

    // add fd to efd
    return epoll_ctl(loop->efd, EPOLL_CTL_ADD, socket, &poll->event);
}

inline int uv_poll_start(uv_poll_t *poll, int events, uv_poll_cb cb) {
    poll->event.events = events;

    // this lookup is "slow" -> µWS only uses a few cbs though!
    poll->cbIndex = cbHead;
    for (int i = 0; i < cbHead; i++) {
        if (callbacks[i] == cb) {
            poll->cbIndex = i;
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
    loops[loop->index] = loop;
    return loop;
}

inline void uv_loop_delete(uv_loop_t *loop) {
    close(loop->efd);
    delete loop;
}

inline void uv_close(uv_handle_t *handle, uv_handle_cb cb) {

}

inline bool uv_is_closing(uv_handle_t *handle) {
    return false;
}

#include <iostream>

inline void uv_run(uv_loop_t *loop, int mode) {

    std::cout << "Size: " << sizeof(uv_poll_t) << std::endl;

    epoll_event readyEvents[64];

    while (true) {
        int numFdReady = epoll_wait(loop->efd, readyEvents, 64, -1);

        //std::cout << "iterating, ready: " << numFdReady << std::endl;

        for (int i = 0; i < numFdReady; i++) {
            uv_poll_t *poll = (uv_poll_t *) readyEvents[i].data.ptr;

            int status = 0;

            callbacks[poll->cbIndex](poll, status, readyEvents[i].events);
        }
    }

    // check if any timer has expired
}

inline int uv_fileno(uv_handle_t *handle) {
    return ((uv_poll_t *) handle)->fd;
}

#endif // UUV_H
