#ifndef WEBSOCKETDATA_H
#define WEBSOCKETDATA_H

#include "WebSocketProtocol.h"
#include "AsyncSocketData.h"

namespace uWS {

// take care with get_ext here !
struct WebSocketData : AsyncSocketData<false>, WebSocketState<true> {
private:

public:
    WebSocketData() : WebSocketState<true>() {
        std::cout << "init websocket data!" << std::endl;
    }
};

}

#endif // WEBSOCKETDATA_H
