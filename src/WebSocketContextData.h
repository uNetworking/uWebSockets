#ifndef WEBSOCKETCONTEXTDATA_H
#define WEBSOCKETCONTEXTDATA_H

#include <functional>
#include <string_view>

namespace uWS {

template <bool, bool> struct WebSocket;

template <bool SSL>
struct WebSocketContextData {

    std::function<void(WebSocket<SSL, true> *, std::string_view)> messageHandler;
};

}

#endif // WEBSOCKETCONTEXTDATA_H
