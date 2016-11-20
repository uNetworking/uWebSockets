#ifndef HTTPSOCKET_UWS_H
#define HTTPSOCKET_UWS_H

#include "Socket.h"

namespace uWS {

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

struct HTTPParser {
    char* headerBegin;
    char* cursor;
    std::pair<char *, size_t> key, value;
    HTTPParser(char *cursor) : cursor(cursor), headerBegin(cursor) {
        size_t length;
        for (; isspace(*cursor); cursor++);
        for (length = 0; !isspace(cursor[length]) && cursor[length] != '\r'; length++);
        key = { cursor, length };
        cursor += length + 1;
        for (length = 0; !isspace(cursor[length]) && cursor[length] != '\r'; length++);
        value = { cursor, length };
    }
    HTTPParser &operator++(int) {
        size_t length = 0;
        for (; !(cursor[0] == '\r' && cursor[1] == '\n'); cursor++);
        cursor += 2;
        if (cursor[0] == '\r' && cursor[1] == '\n') {
            key = value = { 0, 0 };
        } else {
            for (; cursor[length] != ':' && cursor[length] != '\r'; length++);
            key = { cursor, length };
            if (cursor[length] != '\r') {
                cursor += length;
                length = 0;
                while (isspace(*(++cursor)));
                for (; cursor[length] != '\r'; length++);
                value = { cursor, length };
            } else {
                value = { 0, 0 };
            }
        }
        return *this;
    }
};

}

#endif // HTTPSOCKET_UWS_H
