#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include "libusockets.h"
#include "Socket.h"
#include "WebSocketProtocol.h"

// client or server?
template <bool SSL, bool isServer>
struct WebSocket : public Socket<SSL> {

    struct Data : uWS::WebSocketState<isServer> {

    };

    static bool setCompressed(bool enabled) {
        return true;
    }

    static void forceClose(uWS::WebSocketState<true> *wState) {

    }

    static bool handleFragment(char *data, size_t length, unsigned int remainingBytes, int opCode, bool fin, uWS::WebSocketState<isServer> *webSocketState) {

        std::cout << std::string_view(data, length) << std::endl;
    }

    static bool refusePayloadLength(unsigned int length, uWS::WebSocketState<true> *wState) {
        return false;
    }

    void onData(char *data, int length) {
        //Data *webSocketData = (Data *) static_dispatch(us_ssl_socket_ext, us_socket_ext)((SOCKET_TYPE *) this);

        uWS::WebSocketState<isServer> d;

        uWS::WebSocketProtocol<true, WebSocket<SSL, true>>::consume(data, length, &d);


        // todo: this is where the HttpSocket binds together HttpParser and HttpRouter into one
        /*httpData->httpParser.consumePostPadded(data, length, this, [&onHttpRequest](void *user, HttpRequest *httpRequest) {
            onHttpRequest((HttpSocket<SSL> *) user, httpRequest);
        }, [httpData](void *user, std::string_view data) {
            if (httpData->inStream) {
                httpData->inStream(data);
            }
        }, [](void *user) {
            std::cout << "INVALID HTTP!" << std::endl;
        });*/
    }

    WebSocket() = delete;
};

#endif // WEBSOCKET_H
