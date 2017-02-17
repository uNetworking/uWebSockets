#ifndef UUV_H
#define UUV_H

#ifndef USE_MICRO_UV
#include <uv.h>
#define UWS_UV
#else
#define UWS_UV uUV::

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

namespace uUV {

struct uv_handle_t;
struct uv_loop_t;

struct uv_async_t;
struct uv_idle_t;
struct uv_poll_t;
struct uv_timer_t;

// Error codes
const int UV_EINVAL = -1;
const int UV_EBADF = -2;

// Handle types
typedef enum {
    UV_UNKNOWN_HANDLE = 0,
    UV_ASYNC,
    UV_IDLE,
    UV_POLL,
    UV_TIMER,
} uv_handle_type;

// Handle flags
const unsigned int UV_HANDLE_CLOSING = 0x01 << 0;
const unsigned int UV_HANDLE_CLOSED = 0x01 << 1;
const unsigned int UV_HANDLE_RUNNING = 0x01 << 2;

const int UV_WRITABLE = EPOLLOUT;
const int UV_READABLE = EPOLLIN | EPOLLHUP;
const int UV_DISCONNECT = 4; // Not sure which epoll events correspond to disconnect. This value is taken from libuv source code instead.
const int UV_RUN_DEFAULT = 0;
typedef int uv_os_sock_t;
typedef void (*uv_handle_cb)(uv_handle_t *handle);
typedef void (*uv_async_cb)(uv_async_t *handle);
typedef void (*uv_idle_cb)(uv_idle_t *handle);
typedef void (*uv_poll_cb)(uv_poll_t *poll, int status, int events);
typedef void (*uv_timer_cb)(uv_timer_t *handle);

struct uv_handle_t {
    uv_handle_type type;
    unsigned int flags = 0;
    void *data;
    int loopIndex;

    uv_loop_t *get_loop() const;
};

struct uv_loop_t {
    int efd;
    int index;
    std::vector<std::pair<uv_handle_t *, uv_handle_cb>> closing;
    int numEvents;
    int asyncWakeupFd;
    std::mutex async_mutex;
    std::vector<uv_timer_t *> timers;
    std::unordered_set<uv_async_t *> asyncs;
    std::unordered_set<uv_idle_t *> idlers;
    std::chrono::system_clock::time_point timepoint;
};

uv_loop_t *uv_default_loop();
uv_loop_t *uv_loop_new();
void uv_loop_delete(uv_loop_t *loop);

void uv_close(uv_handle_t *handle, uv_handle_cb cb);
bool uv_is_closing(uv_handle_t *handle);

int uv_fileno(uv_handle_t *handle);

struct uv_async_t : uv_handle_t {
    bool run = false;
    uv_async_cb cb;
};

void uv_async_init(uv_loop_t *loop, uv_async_t *async, uv_async_cb cb);
void uv_async_send(uv_async_t *async);

struct uv_idle_t : uv_handle_t {
    uv_idle_cb cb;
};

void uv_idle_init(uv_loop_t *loop, uv_idle_t *idle);
void uv_idle_start(uv_idle_t *idle, uv_idle_cb cb);
void uv_idle_stop(uv_idle_t *idle);

struct uv_poll_t : uv_handle_t {
    epoll_event event;
    int fd;
    unsigned char cbIndex;

    uv_poll_cb get_poll_cb() const;
};

int uv_poll_init_socket(uv_loop_t *loop, uv_poll_t *poll, uv_os_sock_t socket);
int uv_poll_start(uv_poll_t *poll, int events, uv_poll_cb cb);
int uv_poll_stop(uv_poll_t *poll);

struct uv_timer_t : uv_handle_t {
    uv_timer_cb cb;
    std::chrono::system_clock::time_point timepoint;
    int repeat;
};

void uv_timer_init(uv_loop_t *loop, uv_timer_t *timer);
void uv_timer_start(uv_timer_t *timer, uv_timer_cb cb, int timeout, int repeat);
void uv_timer_stop(uv_timer_t *timer);

void uv_run(uv_loop_t *loop, int mode);

} // namespace uWSuv

#endif
#endif // UUV_H
