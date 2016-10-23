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

#include <iostream>

// placeholders here

struct uv_handle_t {
    unsigned char type = 0;
};

struct uv_async_t : uv_handle_t {
    void *data;
};

struct uv_timer_t : uv_handle_t {
    void *data;
};

//#define uv_async_init(x, y, z)
#define uv_async_send(x)
//#define uv_timer_init(x, y)
#define uv_timer_start(x, y, z, w)
#define uv_timer_stop(x)

typedef void (*uv_async_cb)(uv_async_t *handle);



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

static void uv_timer_init(uv_loop_t *loop, uv_timer_t *timer) {
    timer->type = 1;
}

static void uv_async_init(uv_loop_t *loop, uv_async_t *async, uv_async_cb cb) {
    async->type = 2;
}

static uv_loop_t *uv_default_loop() {
    //std::cout << "default loop!" << std::endl;
    return nullptr;
}

// support 128 loops maximum
extern uv_loop_t *loops[128];
extern int loopHead;

// support 128 callbacks maximum
extern uv_poll_cb callbacks[128];
extern int cbHead;

extern int polls;

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
    polls++;
    //std::cout << "uv_poll_init_socket" << std::endl;

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
    poll->fd = socket;
    poll->event.events = 0;
    poll->event.data.ptr = poll;

    // add fd to efd
    return epoll_ctl(loop->efd, EPOLL_CTL_ADD, socket, &poll->event);
}

inline int uv_poll_start(uv_poll_t *poll, int events, uv_poll_cb cb) {

    //std::cout << "uv_poll_start: " << events << std::endl;

    poll->event.events = events;

    // this lookup is "slow" -> µWS only uses a few cbs though!
    poll->cbIndex = cbHead;
    for (int i = 0; i < cbHead; i++) {
        if (callbacks[i] == cb) {
            poll->cbIndex = i;
            break;
        }
    }

    if (poll->cbIndex == cbHead) {
        callbacks[cbHead++] = cb;

        if (cbHead == 128) {
            std::cout << "OVERFLOW!" << std::endl;
        }
    }

    return epoll_ctl(loops[poll->loopIndex]->efd, EPOLL_CTL_MOD, poll->fd, &poll->event);
}

inline int uv_poll_stop(uv_poll_t *poll) {
    //std::cout << "uv_poll_stop" << std::endl;
    return epoll_ctl(loops[poll->loopIndex]->efd, EPOLL_CTL_DEL, poll->fd, &poll->event);
}

inline uv_loop_t *uv_loop_new() {
    //std::cout << "uv_loop_new" << std::endl;
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

#include <vector>

extern std::vector<std::pair<uv_handle_t *, uv_handle_cb>> closing;

inline void uv_close(uv_handle_t *handle, uv_handle_cb cb) {
    if (handle->type == 0) {
        uv_poll_t *poll = (uv_poll_t *) handle;
        poll->fd = -1;
        closing.push_back({handle, cb});
    }
}

inline bool uv_is_closing(uv_handle_t *handle) {
    return ((uv_poll_t *) handle)->fd == -1;
}

#include <iostream>

inline void uv_run(uv_loop_t *loop, int mode) {

    //std::cout << "Size: " << sizeof(uv_poll_t) << std::endl;

    epoll_event readyEvents[64];

    while (polls) {


        for (std::pair<uv_handle_t *, uv_handle_cb> c : closing) {
            //std::cout << "CLOSING POLL" << std::endl;
            polls--;
            c.second(c.first);

            if (!polls) {
                closing.clear();

                // bra läge att tömma alla states här
                //cbHead = 0;

                return;
            }
        }

        closing.clear();

        int numFdReady = epoll_wait(loop->efd, readyEvents, 64, -1);
        for (int i = 0; i < numFdReady; i++) {
            uv_poll_t *poll = (uv_poll_t *) readyEvents[i].data.ptr;
            int status = -bool(readyEvents[i].events & EPOLLERR);
            callbacks[poll->cbIndex](poll, status, readyEvents[i].events);
        }
    }

    // check if any timer has expired
}

inline int uv_fileno(uv_handle_t *handle) {
    return ((uv_poll_t *) handle)->fd;
}

#endif // UUV_H
