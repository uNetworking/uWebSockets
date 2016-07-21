#include "EventSystem.h"

namespace uWS {

EventSystem::EventSystem(LoopType loopType) : loopType(loopType)
{
    loop = loopType == MASTER ? uv_default_loop() : uv_loop_new();
}

EventSystem::~EventSystem()
{
    if (loopType == WORKER) {
        uv_loop_delete(loop);
    }
}

void EventSystem::run()
{
    uv_run(loop, UV_RUN_DEFAULT);
}

}
