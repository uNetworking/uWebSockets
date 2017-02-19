#include "uUV.h"

#ifndef USE_MICRO_UV
void uv_close(uv_async_t *handle, uv_close_cb cb) {
    uv_close((uv_handle_t *) handle, cb);
}
void uv_close(uv_idle_t *handle, uv_close_cb cb) {
    uv_close((uv_handle_t *) handle, cb);
}
void uv_close(uv_poll_t *handle, uv_close_cb cb) {
    uv_close((uv_handle_t *) handle, cb);
}
void uv_close(uv_timer_t *handle, uv_close_cb cb) {
    uv_close((uv_handle_t *) handle, cb);
}

bool uv_is_closing(uv_async_t *handle) {
    return uv_is_closing((uv_handle_t *) handle);
}
bool uv_is_closing(uv_idle_t *handle) {
    return uv_is_closing((uv_handle_t *) handle);
}
bool uv_is_closing(uv_poll_t *handle) {
    return uv_is_closing((uv_handle_t *) handle);
}
bool uv_is_closing(uv_timer_t *handle) {
    return uv_is_closing((uv_handle_t *) handle);
}
#else

#include <sys/eventfd.h>

//namespace uUV {

uv_loop_t *loops[128];
int loopHead = 0;

#define CALLBACK_ARR_SIZE 128
uv_async_cb async_callbacks[CALLBACK_ARR_SIZE];
int asyncCbHead = 0;
uv_idle_cb idle_callbacks[CALLBACK_ARR_SIZE];
int idleCbHead = 0;
uv_poll_cb poll_callbacks[CALLBACK_ARR_SIZE];
int pollCbHead = 0;
uv_timer_cb timer_callbacks[CALLBACK_ARR_SIZE];
int timerCbHead = 0;

uv_loop_t *uv_handle_t::get_loop() const {
    return loops[loopIndex];
}

inline uv_loop_t *uv_loop_helper() {
    uv_loop_t *loop = new uv_loop_t;
    loop->efd = epoll_create(1);
    loop->index = loopHead++;
    loop->numEvents = 0;

    loop->asyncWakeupFd = eventfd(0, EFD_SEMAPHORE | EFD_NONBLOCK);
    struct epoll_event wakeupEvents;
    wakeupEvents.events = EPOLLHUP | EPOLLERR | EPOLLIN | EPOLLET;
    wakeupEvents.data.ptr = nullptr;
    epoll_ctl(loop->efd, EPOLL_CTL_ADD, loop->asyncWakeupFd, &wakeupEvents);

    loops[loop->index] = loop;
    return loop;
}

inline void init() {
    uv_loop_helper();
}

uv_loop_t *uv_default_loop() {
    if (!loopHead) {
        init();
    }
    return loops[0];
}

uv_loop_t *uv_loop_new() {
    if (!loopHead)
        init();
    return uv_loop_helper();
}

void uv_loop_delete(uv_loop_t *loop) {
    epoll_ctl(loop->efd, EPOLL_CTL_DEL, loop->asyncWakeupFd, nullptr);
    close(loop->efd);
    loops[loop->index] = nullptr;
    delete loop;
}

void uv_async_init(uv_loop_t *loop, uv_async_t *async, uv_async_cb cb) {
    async->loopIndex = loop->index;
    loop->numEvents++;

    async->cbIndex = asyncCbHead;
    for (int i = 0; i < asyncCbHead; i++) {
        if (async_callbacks[i] == cb) {
            async->cbIndex = i;
            break;
        }
    }
    if (async->cbIndex == asyncCbHead) {
        async_callbacks[asyncCbHead++] = cb;
    }

    loop->asyncs.insert(async);
}

void uv_async_send(uv_async_t *async) {
    uv_loop_t *loop = async->get_loop();
    loop->async_mutex.lock();
    uint64_t val = 1;
    int w = write(loop->asyncWakeupFd, &val, sizeof(val));
    async->run = true;
    loop->async_mutex.unlock();
}

void uv_close(uv_async_t *handle, uv_close_cb cb) {
    uv_loop_t *loop = handle->get_loop();

    loop->asyncs.erase((uv_async_t *) handle);

    handle->flags |= UV_HANDLE_CLOSING;
    loop->closing.push_back({(uv_handle_t *) handle, cb});
}

bool uv_is_closing(uv_async_t *handle) {
    return handle->flags & (UV_HANDLE_CLOSING | UV_HANDLE_CLOSED);
}

void uv_idle_init(uv_loop_t *loop, uv_idle_t *idle) {
    idle->loopIndex = loop->index;
    loop->numEvents++;
}

void uv_idle_start(uv_idle_t *idle, uv_idle_cb cb) {
    idle->cbIndex = idleCbHead;
    for (int i = 0; i < idleCbHead; i++) {
        if (idle_callbacks[i] == cb) {
            idle->cbIndex = i;
            break;
        }
    }
    if (idle->cbIndex == idleCbHead) {
        idle_callbacks[idleCbHead++] = cb;
    }

    idle->get_loop()->idlers.insert(idle);
}

void uv_idle_stop(uv_idle_t *idle) {
    idle->get_loop()->idlers.erase(idle);
}

void uv_close(uv_idle_t *handle, uv_close_cb cb) {
    uv_loop_t *loop = handle->get_loop();
    handle->flags |= UV_HANDLE_CLOSING;
    loop->closing.push_back({(uv_handle_t *) handle, cb});
}

bool uv_is_closing(uv_idle_t *handle) {
    return handle->flags & (UV_HANDLE_CLOSING | UV_HANDLE_CLOSED);
}

uv_poll_cb uv_poll_t::get_poll_cb() const {
    return poll_callbacks[cbIndex];
}

int uv_poll_init_socket(uv_loop_t *loop, uv_poll_t *poll, uv_os_sock_t socket) {
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
    loop->numEvents++;
    return epoll_ctl(loop->efd, EPOLL_CTL_ADD, socket, &poll->event);
}

int uv_poll_start(uv_poll_t *poll, int events, uv_poll_cb cb) {
    poll->event.events = events;
    poll->cbIndex = pollCbHead;
    for (int i = 0; i < pollCbHead; i++) {
        if (poll_callbacks[i] == cb) {
            poll->cbIndex = i;
            break;
        }
    }
    if (poll->cbIndex == pollCbHead) {
        poll_callbacks[pollCbHead++] = cb;
    }
    return epoll_ctl(poll->get_loop()->efd, EPOLL_CTL_MOD, poll->fd, &poll->event);
}

int uv_poll_stop(uv_poll_t *poll) {
    return epoll_ctl(poll->get_loop()->efd, EPOLL_CTL_DEL, poll->fd, &poll->event);
}

void uv_close(uv_poll_t *handle, uv_close_cb cb) {
    uv_loop_t *loop = handle->get_loop();

    uv_poll_t *poll = (uv_poll_t *) handle;
    poll->fd = -1;

    loop->closing.push_back({(uv_handle_t *) handle, cb});
}

bool uv_is_closing(uv_poll_t *handle) {
    return handle->fd == -1;
}

int uv_fileno(uv_poll_t *handle) {
	return handle->fd;
}

void uv_timer_init(uv_loop_t *loop, uv_timer_t *timer) {
    timer->loopIndex = loop->index;
    loop->numEvents++;
    loop->timepoint = std::chrono::system_clock::now();
}

void uv_timer_enqueue(uv_timer_t *timer, int timeout) {
    timer->timepoint = timer->get_loop()->timepoint + std::chrono::milliseconds(timeout);
    // sort timers from farthest to soonest so we can pop from back in O(1)
    uv_loop_t *loop = timer->get_loop();
    if (loop->timers.size() && timeout) {
        loop->timers.insert(
            std::upper_bound(loop->timers.begin(), loop->timers.end(), timer, [](uv_timer_t* a, uv_timer_t* b) {
                return a->timepoint > b->timepoint;
            }),
            timer   
        );
    }
    else
        loop->timers.push_back(timer);
}
void uv_timer_start(uv_timer_t *timer, uv_timer_cb cb, int timeout, int repeat) {
    timer->cbIndex = timerCbHead;
    for (int i = 0; i < timerCbHead; i++) {
        if (timer_callbacks[i] == cb) {
            timer->cbIndex = i;
            break;
        }
    }
    if (timer->cbIndex == timerCbHead) {
        timer_callbacks[timerCbHead++] = cb;
    }

    timer->repeat = repeat;
    timer->flags = UV_HANDLE_RUNNING;
    uv_timer_enqueue(timer, timeout);
}

void uv_timer_stop(uv_timer_t *timer) {
    timer->flags &= ~UV_HANDLE_RUNNING;
    uv_loop_t *loop = timer->get_loop();
    for (int i = 0; i < loop->timers.size(); ++i)
        if (loop->timers[i] == timer)
        {
            loop->timers.erase(loop->timers.begin() + i);
            break;
        }
}

void uv_close(uv_timer_t *handle, uv_close_cb cb) {
    uv_loop_t *loop = handle->get_loop();
    handle->flags |= UV_HANDLE_CLOSING;
    loop->closing.push_back({(uv_handle_t *) handle, cb});
}

bool uv_is_closing(uv_timer_t *handle) {
    return handle->flags & (UV_HANDLE_CLOSING | UV_HANDLE_CLOSED);
}

void uv_run(uv_loop_t *loop, int mode) {
    loop->timepoint = std::chrono::system_clock::now();
    signal(SIGPIPE, SIG_IGN);
    int iter = 0;
    while (loop->numEvents) {
        ++iter;
        // Close any events that are ready to close
        if (loop->closing.size()) {
            // Make a copy so that its ok to call uv_close in the callbacks
            std::vector<std::pair<uv_handle_t *, uv_close_cb>> closingCopy = loop->closing;
            loop->closing.clear();
            
            for (std::pair<uv_handle_t *, uv_close_cb> c : closingCopy) {
                loop->numEvents--;
                c.first->flags &= ~UV_HANDLE_CLOSING;
                c.first->flags |= UV_HANDLE_CLOSED;
                c.second(c.first);
            }
        }

        // Wait for events to be ready
        loop->timepoint = std::chrono::system_clock::now();
        int delay = -1;
        if (loop->idlers.size()) {
            delay = 0;
        } else if (loop->timers.size()) {
            delay = std::max<int>(std::chrono::duration_cast<std::chrono::milliseconds>(loop->timers.back()->timepoint - loop->timepoint).count(), 0);
        }
        epoll_event readyEvents[1024];
        int numFdReady = epoll_wait(loop->efd, readyEvents, 1024, delay);

        // Handle polling events
        for (int i = 0; i < numFdReady; i++) {
            uv_poll_t *poll = (uv_poll_t *) readyEvents[i].data.ptr;
            if (poll) {
                int status = -bool(readyEvents[i].events & EPOLLERR);
                poll_callbacks[poll->cbIndex](poll, status, readyEvents[i].events);
            } else { // async wakeup event has nullptr
                loop->async_mutex.lock();
                uint64_t val;
                int r = read(loop->asyncWakeupFd, &val, sizeof(val));
                loop->async_mutex.unlock();
            }
        }

        // Handle async events
        if (loop->asyncs.size()) {
            std::vector<uv_async_t *> readyAsyncs;
            // Find ready asyncs first so we can safely modify the set inside callbacks
            loop->async_mutex.lock();
            for (uv_async_t *async : loop->asyncs)
                if (async->run)
                {
                    async->run = false;
                    readyAsyncs.push_back(async);
                }
            loop->async_mutex.unlock();
            for (uv_async_t *async : readyAsyncs)
				async_callbacks[async->cbIndex](async);
        }

        // Handle idle events
        if (loop->idlers.size()) {
            std::unordered_set<uv_idle_t *> readyIdlers = loop->idlers;
            for (uv_idle_t *idle : readyIdlers)
				idle_callbacks[idle->cbIndex](idle);
        }

        // Handle timer events
        if (loop->timers.size()) {
            loop->timepoint = std::chrono::system_clock::now();
            // Copy ready timers to separate vector so callbacks can safely modify the original
            std::vector<uv_timer_t *> readyTimers;
            while (loop->timers.size() && std::chrono::duration_cast<std::chrono::milliseconds>(loop->timers.back()->timepoint - loop->timepoint).count() <= 0) {
                readyTimers.push_back(loop->timers.back());
                loop->timers.pop_back();
            }
            for (uv_timer_t* timer : readyTimers) {
                if (timer->flags & UV_HANDLE_RUNNING) {
                    timer_callbacks[timer->cbIndex](timer);
                    // Have to check for running again in case timer was stopped in callback
                    if (timer->repeat && timer->flags & UV_HANDLE_RUNNING) {
                        uv_timer_enqueue(timer, timer->repeat);
                    }
                }
            }
        }
    }
}

//}
#endif
