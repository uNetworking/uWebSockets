#ifndef HTTPSOCKET_UWS_H
#define HTTPSOCKET_UWS_H

#include "Socket.h"

namespace uWS {

enum ContentType {
    TEXT_HTML
};

template <const bool isServer>
struct WIN32_EXPORT HTTPSocket : private uS::Socket {
    struct Data : uS::SocketData {
        std::string httpBuffer;

        // todo: limit these to only client, but who cares for now?
        std::string path;
        std::string host;
        void *httpUser;

        Data(uS::SocketData *socketData) : uS::SocketData(*socketData) {}
    };

    uv_poll_t *getPollHandle() const {return p;}
    void respond(char *message, size_t length, ContentType contentType, void(*callback)(void *webSocket, void *data, bool cancelled, void *reserved) = nullptr, void *callbackData = nullptr);
    using uS::Socket::shutdown;
    using uS::Socket::close;

    HTTPSocket(uS::Socket s) : uS::Socket(s) {}
    typename HTTPSocket::Data *getData() {
        return (HTTPSocket::Data *) getSocketData();
    }

    bool upgrade(const char *secKey = nullptr, const char *extensions = nullptr,
                 size_t extensionsLength = 0, const char *subprotocol = nullptr,
                 size_t subprotocolLength = 0);

private:
    friend class uS::Socket;
    friend struct Hub;
    static void onData(uS::Socket s, char *data, int length);
    static void onEnd(uS::Socket s);
};

}

#endif // HTTPSOCKET_UWS_H
