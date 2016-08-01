#include "EventSystem.h"
#include "WebSocket.h"

namespace uWS {

void EventSystem::changePollAsync(uv_poll_t *p)
{
    pollsToChangeMutex.lock();
    pollsToChange.push_back(p);
    pollsToChangeMutex.unlock();
    uv_async_send(asyncPollChange);
}

EventSystem::EventSystem(LoopType loopType) : loopType(loopType)
{
    loop = loopType == MASTER ? uv_default_loop() : uv_loop_new();

    if (loopType == WORKER) {
        asyncPollChange = new uv_async_t;
        asyncPollChange->data = this;
        uv_async_init(loop, asyncPollChange, [](uv_async_t *a) {
            EventSystem *es = (EventSystem *) a->data;

            es->pollsToChangeMutex.lock();
            for (uv_poll_t *p : es->pollsToChange) {
                uv_poll_start(p, UV_WRITABLE | UV_READABLE, WebSocket::onWritableReadable);
            }
            es->pollsToChange.clear();
            es->pollsToChangeMutex.unlock();
        });
    }
}

EventSystem::~EventSystem()
{
    if (loopType == WORKER) {
        uv_loop_delete(loop);
    }
}

void EventSystem::run()
{
    tid = pthread_self();
    uv_run(loop, UV_RUN_DEFAULT);
}

}
