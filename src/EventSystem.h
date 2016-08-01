#ifndef EVENTSYSTEM_H
#define EVENTSYSTEM_H

#include <uv.h>
#include <vector>
#include <mutex>
#include "Network.h"

namespace uWS {

enum LoopType {
    MASTER,
    WORKER
};

class WIN32_EXPORT EventSystem
{
    friend class Server;
    friend class WebSocket;
    LoopType loopType;
    uv_loop_t *loop;
    uv_async_t *asyncPollChange;
    std::vector<uv_poll_t *> pollsToChange;
    std::mutex pollsToChangeMutex;
    pthread_t tid;
    bool threadSafe;

    void changePollAsync(uv_poll_t *p);

public:
    EventSystem(LoopType loopType = MASTER, bool threadSafe = false);
    ~EventSystem();
    void run();
};

}

#endif // EVENTSYSTEM_H
