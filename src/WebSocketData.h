#ifndef WEBSOCKETDATA_H
#define WEBSOCKETDATA_H

#include "WebSocketProtocol.h"
#include "AsyncSocketData.h"

#include <string>

namespace uWS {

// take care with get_ext here !
struct WebSocketData : AsyncSocketData<false>, WebSocketState<true> {
    template <bool, bool> friend struct WebSocketContext;
    template <bool, bool> friend struct WebSocket;
private:
    std::string fragmentBuffer;
    int controlTipLength = 0;
    bool isShuttingDown = 0;
public:
    WebSocketData() : WebSocketState<true>() {
        std::cout << "init websocket data!" << std::endl;
    }
};

}

#endif // WEBSOCKETDATA_H
