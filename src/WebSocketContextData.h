#ifndef WEBSOCKETCONTEXTDATA_H
#define WEBSOCKETCONTEXTDATA_H

#include <functional>
#include <string_view>

#include "WebSocketProtocol.h"

namespace uWS {

template <bool, bool> struct WebSocket;

template <bool SSL>
struct WebSocketContextData {

    std::function<void(WebSocket<SSL, true> *, std::string_view, uWS::OpCode)> messageHandler;
};

}

#endif // WEBSOCKETCONTEXTDATA_H
