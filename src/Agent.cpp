#include "Agent.h"

namespace uWS {

template<class WS_T>
void Agent<WS_T>::onConnection(std::function<void (WS_T)> connectionCallback)
{
    this->connectionCallback = connectionCallback;
}

template<class WS_T>
void Agent<WS_T>::onDisconnection(std::function<void (WS_T, int, char *, size_t)> disconnectionCallback)
{
    this->disconnectionCallback = disconnectionCallback;
}

template<class WS_T>
void Agent<WS_T>::onMessage(std::function<void (WS_T, char *, size_t, OpCode)> messageCallback)
{
    this->messageCallback = messageCallback;
}

template<class WS_T>
void Agent<WS_T>::onPing(std::function<void (WS_T, char *, size_t)> pingCallback)
{
    this->pingCallback = pingCallback;
}

template<class WS_T>
void Agent<WS_T>::onPong(std::function<void (WS_T, char *, size_t)> pongCallback)
{
    this->pongCallback = pongCallback;
}

template class Agent<WebSocket<SERVER>>;

}
