// Rename uUV.h / uUV.cpp -> Backend.h/.cpp

#ifndef UUV_H
#define UUV_H

// Use libuv by default
//#define USE_LIBUV

// Libuv backend (should be moved out to libuv.h)
#ifdef USE_LIBUV

#include <uv.h>

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

struct Async {
    uv_async_t uv_async;

    Async(Loop *loop) {
        uv_async.loop = loop;
    }

    void start(void (*cb)(Async *)) {
        uv_async_init(uv_async.loop, &uv_async, (uv_async_cb) cb);
    }

    void send() {
        uv_async_send(&uv_async);
    }

    void close(uv_close_cb cb) {
        uv_close((uv_handle_t *) &uv_async, cb);
    }

    void setData(void *data) {
        uv_async.data = data;
    }

    void *getData() {
        return uv_async.data;
    }
};

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

// Raw epoll implementation
#else

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

// obviously high prio
struct Loop {

    int epfd;
    unsigned char index;
    int numPolls = 0;
    epoll_event readyEvents[1024];
    std::chrono::system_clock::time_point timepoint;
    std::vector<Timepoint> timers;

    Loop(bool defaultLoop) {
        std::cout << "Loop::Loop(bool)" << std::endl;
        epfd = epoll_create(1);
        loops[index = loopHead++] = this;
    }

    static Loop *createLoop(bool defaultLoop = true) {
        return new Loop(defaultLoop);
    }

    void destroy() {
        ::close(epfd);
    }

    void run();

    int getEpollFd() {
        return epfd;
    }
};

// low prio, not used very often
struct Async {

    Async(Loop *loop) {

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

// medium prio, can be skipped for a while
struct Timer {

    Loop *loop = nullptr;
    void *data;

    Timer(Loop *loop) {
        std::cout << "Timer::init" << std::endl;
        this->loop = loop;
    }

    void start(void (*cb)(Timer *), int timeout, int repeat) {
        std::cout << "Timer::start" << std::endl;
        std::chrono::system_clock::time_point timepoint = loop->timepoint + std::chrono::milliseconds(timeout);

        loop->timers.push_back({cb, this, timepoint, repeat});
        std::sort(loop->timers.begin(), loop->timers.end(), [](const Timepoint &a, const Timepoint &b) {
            return a.timepoint < b.timepoint;
        });
    }

    void setData(void *data) {
        std::cout << "Timer::setData" << std::endl;
        this->data = data;
    }

    void *getData() {
        std::cout << "Timer::getData" << std::endl;
        return data;
    }

    void stop() {
        std::cout << "Timer::stop" << std::endl;
        auto pos = loop->timers.begin();
        for (Timepoint &t : loop->timers) {
            if (t.timer == this) {
                loop->timers.erase(pos);
                break;
            }
            pos++;
        }
    }

    void close(uv_close_cb cb) {
        std::cout << "Timer::close" << std::endl;
    }
};

// 32 bytes
struct Poll {
    epoll_event event; // 8 bytes
    void *data; // 8 bytes

    // 4 bytes
    /*struct {
        unsigned int fd : 23; // 23 bit is 8 million fds
    };*/

    int fd = -1; // 4 bytes


    unsigned char loopIndex, cbIndex; // 2 bytes (leaves 2 bytes padding)

    Poll(Loop *loop, uv_os_sock_t fd) {
        init(loop, fd);
    }

    void init(Loop *loop, uv_os_sock_t fd) {
        std::cout << "Poll::init" << std::endl;
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
        std::cout << "Poll::setData" << std::endl;
        this->data = data;
    }

    bool isClosing() {
        std::cout << "Poll::isClosing" << std::endl;
        return false;
    }

    uv_os_sock_t getFd() {
        std::cout << "Poll::getFd" << std::endl;
        return fd;
    }

    void *getData() {
        std::cout << "Poll::getData" << std::endl;
        return data;
    }

    void setCb(void (*cb)(Poll *p, int status, int events)) {
        std::cout << "Poll::setCb" << std::endl;
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
        std::cout << "Poll::start" << std::endl;
        event.events = events;
        epoll_ctl(loops[loopIndex]->epfd, EPOLL_CTL_ADD, fd, &event);
    }

    void change(int events) {
        std::cout << "Poll::change" << std::endl;
        event.events = events;
        epoll_ctl(loops[loopIndex]->epfd, EPOLL_CTL_MOD, fd, &event);
    }

    void stop() {
        std::cout << "Poll::stop" << std::endl;
        epoll_ctl(loops[loopIndex]->epfd, EPOLL_CTL_DEL, fd, &event);
    }

    void close(uv_close_cb cb) {
        std::cout << "Poll::close" << std::endl;
    }

    void (*getPollCb())(Poll *, int, int) {
        return (void (*)(Poll *, int, int)) nullptr;
    }

    Loop *getLoop() {
        std::cout << "Poll::getLoop" << std::endl;
        return loops[loopIndex];
    }
};

// just listing some code never reached / to pick from
#ifdef USE_MICRO_UV

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

struct uv_handle_t;
struct uv_loop_t;

struct Async;
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
typedef void (*uv_async_cb)(Async *handle);
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
    std::unordered_set<Async *> asyncs;
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
struct Async : uv_handle_t {
    unsigned char cbIndex;
    bool run = false;
};

void uv_async_init(uv_loop_t *loop, Async *async, uv_async_cb cb);
void uv_async_send(Async *async);
void uv_close(Async *handle, uv_close_cb cb);
bool uv_is_closing(Async *handle);

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
#endif

#endif
#endif // UUV_H
