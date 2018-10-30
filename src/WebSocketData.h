#ifndef WEBSOCKETDATA_H
#define WEBSOCKETDATA_H

#include "WebSocketProtocol.h"

namespace uWS {

struct WebSocketData : WebSocketState<true> {
private:

public:
    WebSocketData() : WebSocketState<true>() {
        std::cout << "init websocket data!" << std::endl;
    }
};

}

#endif // WEBSOCKETDATA_H
