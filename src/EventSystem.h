#ifndef EVENTSYSTEM_H
#define EVENTSYSTEM_H

#include <uv.h>

namespace uWS {

enum LoopType {
    MASTER,
    WORKER
};

class EventSystem
{
    friend class Server;
    LoopType loopType;
    uv_loop_t *loop;

public:
    EventSystem(LoopType loopType = MASTER);
    ~EventSystem();
    void run();
};

}

#endif // EVENTSYSTEM_H
