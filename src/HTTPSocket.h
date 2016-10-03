#ifndef HTTPSOCKET_H
#define HTTPSOCKET_H

#include "Socket.h"

namespace uWS {

template <const bool isServer>
struct HTTPSocket : private uS::Socket {
    struct Data : uS::SocketData {
        std::string httpBuffer;

        // todo: limit these to only client, but who cares for now?
        std::string path;
        std::string host;
        void *httpUser;

        Data(uS::SocketData *socketData) : uS::SocketData(*socketData) {}
    };

    HTTPSocket(uS::Socket s) : uS::Socket(s) {}
    HTTPSocket::Data *getData() {
        return (HTTPSocket::Data *) getSocketData();
    }

    bool upgrade(const char *secKey = nullptr, const char *extensions = nullptr,
                 size_t extensionsLength = 0, const char *subprotocol = nullptr,
                 size_t subprotocolLength = 0);

private:
    friend class uS::Socket;
    friend class Hub;
    static void onData(uS::Socket s, char *data, int length);
    static void onEnd(uS::Socket s);
};

}

#endif // HTTPSOCKET_H
