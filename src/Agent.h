#ifndef AGENT_H
#define AGENT_H

#include <functional>
#include <uv.h>

#include "WebSocket.h"

namespace uWS {

template<class WS_T>
class Agent
{
protected:
    std::function<void(uv_os_sock_t, const char *, void *, const char *, size_t)> upgradeCallback;
    std::function<void(WS_T)> connectionCallback;
    std::function<void(WS_T, int code, char *message, size_t length)> disconnectionCallback;
    std::function<void(WS_T, char *, size_t, OpCode)> messageCallback;
    std::function<void(WS_T, char *, size_t)> pingCallback;
    std::function<void(WS_T, char *, size_t)> pongCallback;
public:
    void onConnection(std::function<void(WS_T)> connectionCallback);
    void onDisconnection(std::function<void(WS_T, int code, char *message, size_t length)> disconnectionCallback);
    void onMessage(std::function<void(WS_T, char *, size_t, OpCode)> messageCallback);
    void onPing(std::function<void(WS_T, char *, size_t)> pingCallback);
    void onPong(std::function<void(WS_T, char *, size_t)> pongCallback);
};

}

#endif // AGENT_H
