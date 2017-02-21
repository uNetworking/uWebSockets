#ifndef UUV_H
#define UUV_H

#ifndef USE_MICRO_UV
#include <uv.h>

// Loop implemented with uv_loop_t
struct Loop : uv_loop_t {
    static Loop *createLoop(bool defaultLoop = true) {
        if (defaultLoop) {
            return (Loop *) uv_default_loop();
        } else {
            return (Loop *) uv_loop_new();
        }
    }

    void destroy() {
        if (this != uv_default_loop()) {
            uv_loop_delete(this);
        }
    }

    void run() {
        uv_run(this, UV_RUN_DEFAULT);
    }
};

// Timer implemented with uv_timer_t
struct Timer {
    uv_timer_t uv_timer;

    Timer(Loop *loop) {
        uv_timer_init(loop, &uv_timer);
    }

    void start(void (*cb)(Timer *), int first, int repeat) {
        uv_timer_start(&uv_timer, (uv_timer_cb) cb, first, repeat);
    }

    void setData(void *data) {
        uv_timer.data = data;
    }

    void *getData() {
        return uv_timer.data;
    }

    void stop() {
        uv_timer_stop(&uv_timer);
    }

    void close(uv_close_cb cb) {
        uv_close((uv_handle_t *) &uv_timer, cb);
    }
};

// Poll implemented with uv_poll_t
struct Poll {
    uv_poll_t uv_poll;

    Poll(Loop *loop, uv_os_sock_t fd) {
        init(loop, fd);
    }

    void init(Loop *loop, uv_os_sock_t fd) {
        uv_poll_init_socket(loop, &uv_poll, fd);
    }

    Poll() {

    }

    ~Poll() {
    }

    void setData(void *data) {
        uv_poll.data = data;
    }

    bool isClosing() {
        return uv_is_closing((uv_handle_t *) &uv_poll);
    }

    uv_os_sock_t getFd() {
#ifdef _WIN32
        uv_os_sock_t fd;
        uv_fileno((uv_handle_t *) &uv_poll, (uv_os_fd_t *) &fd);
        return fd;
#else
        return uv_poll.io_watcher.fd;
#endif
    }

    void *getData() {
        return uv_poll.data;
    }

    void setCb(void (*cb)(Poll *p, int status, int events)) {
        uv_poll.poll_cb = (uv_poll_cb) cb;
    }

    void start(int events) {
        uv_poll_start(&uv_poll, events, uv_poll.poll_cb);
    }

    void change(int events) {
        uv_poll_start(&uv_poll, events, uv_poll.poll_cb);
    }

    void stop() {
        uv_poll_stop(&uv_poll);
    }

    void close(uv_close_cb cb) {
        uv_close((uv_handle_t *) &uv_poll, cb);
    }

    void (*getPollCb())(Poll *, int, int) {
        return (void (*)(Poll *, int, int)) uv_poll.poll_cb;
    }

    Loop *getLoop() {
        return (Loop *) uv_poll.loop;
    }
};

void uv_close(uv_async_t *handle, uv_close_cb cb);
bool uv_is_closing(uv_async_t *handle);

#else

#include <arpa/inet.h>
#include <fcntl.h>
#include <mutex>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define UV_VERSION_MINOR 3

#include <algorithm>
#include <chrono>
#include <vector>
#include <unordered_set>

//namespace uUV {

struct uv_handle_t;
struct uv_loop_t;

struct uv_async_t;
struct uv_idle_t;
struct uv_poll_t;
struct uv_timer_t;

// Error codes
const int UV_EINVAL = -1;
const int UV_EBADF = -2;

// Handle flags
const unsigned char UV_HANDLE_CLOSING = 0x01 << 0;
const unsigned char UV_HANDLE_CLOSED = 0x01 << 1;
const unsigned char UV_HANDLE_RUNNING = 0x01 << 2;

const int UV_WRITABLE = EPOLLOUT;
const int UV_READABLE = EPOLLIN | EPOLLHUP;
const int UV_DISCONNECT = 4; // Not sure which epoll events correspond to disconnect. This value is taken from libuv source code instead.
const int UV_RUN_DEFAULT = 0;
typedef int uv_os_sock_t;
typedef void (*uv_close_cb)(uv_handle_t *handle);
typedef void (*uv_async_cb)(uv_async_t *handle);
typedef void (*uv_idle_cb)(uv_idle_t *handle);
typedef void (*uv_poll_cb)(uv_poll_t *poll, int status, int events);
typedef void (*uv_timer_cb)(uv_timer_t *handle);

/*
 * All struct members are specifically ordered to optimize packing! Be careful
 * when making changes.
 *
 * Member size reference:
 * std::mutex .................................... 40 bytes
 * std::chrono::system_clock::time_point ......... 8 bytes
 * std::vector ................................... 24 bytes
 * std::unordered_set ............................ 56 bytes
 * epoll_event ................................... 12 bytes
 */

// 16 bytes
struct uv_handle_t {
    void *data;
    unsigned char flags = 0;
    unsigned char loopIndex;

    uv_loop_t *get_loop() const;
};

// 224 bytes
struct uv_loop_t {
    std::unordered_set<uv_async_t *> asyncs;
    std::unordered_set<uv_idle_t *> idlers;
    std::vector<uv_timer_t *> timers;
    std::vector<std::pair<uv_handle_t *, uv_close_cb>> closing;
    std::mutex async_mutex;
    std::chrono::system_clock::time_point timepoint;
    int efd;
    int index;
    int numEvents;
    int asyncWakeupFd;
};

uv_loop_t *uv_default_loop();
uv_loop_t *uv_loop_new();
void uv_loop_delete(uv_loop_t *loop);


// 16 bytes
struct uv_async_t : uv_handle_t {
    unsigned char cbIndex;
    bool run = false;
};

void uv_async_init(uv_loop_t *loop, uv_async_t *async, uv_async_cb cb);
void uv_async_send(uv_async_t *async);
void uv_close(uv_async_t *handle, uv_close_cb cb);
bool uv_is_closing(uv_async_t *handle);

// 16 bytes
struct uv_idle_t : uv_handle_t {
    unsigned char cbIndex;
};

void uv_idle_init(uv_loop_t *loop, uv_idle_t *idle);
void uv_idle_start(uv_idle_t *idle, uv_idle_cb cb);
void uv_idle_stop(uv_idle_t *idle);
void uv_close(uv_idle_t *handle, uv_close_cb cb);
bool uv_is_closing(uv_idle_t *handle);

// 32 bytes
struct uv_poll_t : uv_handle_t {
    unsigned char cbIndex;
    int fd;
    epoll_event event;

    uv_poll_cb get_poll_cb() const;
};

int uv_poll_init_socket(uv_loop_t *loop, uv_poll_t *poll, uv_os_sock_t socket);
int uv_poll_start(uv_poll_t *poll, int events, uv_poll_cb cb);
int uv_poll_stop(uv_poll_t *poll);
void uv_close(uv_poll_t *handle, uv_close_cb cb);
bool uv_is_closing(uv_poll_t *handle);
int uv_fileno(uv_poll_t *handle);

// 24 bytes
struct uv_timer_t : uv_handle_t {
    unsigned char cbIndex;
    int repeat;
    std::chrono::system_clock::time_point timepoint;
};

void uv_timer_init(uv_loop_t *loop, uv_timer_t *timer);
void uv_timer_start(uv_timer_t *timer, uv_timer_cb cb, int timeout, int repeat);
void uv_timer_stop(uv_timer_t *timer);
void uv_close(uv_timer_t *handle, uv_close_cb cb);
bool uv_is_closing(uv_timer_t *handle);

void uv_run(uv_loop_t *loop, int mode);

//} // namespace uUV

#endif
#endif // UUV_H
