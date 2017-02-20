#include "Hub.h"
#include "HTTPSocket.h"
#include <openssl/sha.h>

static const int INFLATE_LESS_THAN_ROUGHLY = 16777216;

namespace uWS {

char *Hub::inflate(char *data, size_t &length) {
    dynamicInflationBuffer.clear();

    inflationStream.next_in = (Bytef *) data;
    inflationStream.avail_in = length;

    int err;
    do {
        inflationStream.next_out = (Bytef *) inflationBuffer;
        inflationStream.avail_out = LARGE_BUFFER_SIZE;
        err = ::inflate(&inflationStream, Z_FINISH);
        if (!inflationStream.avail_in) {
            break;
        }

        dynamicInflationBuffer.append(inflationBuffer, LARGE_BUFFER_SIZE - inflationStream.avail_out);
    } while (err == Z_BUF_ERROR && dynamicInflationBuffer.length() <= INFLATE_LESS_THAN_ROUGHLY);

    inflateReset(&inflationStream);

    if ((err != Z_BUF_ERROR && err != Z_OK) || dynamicInflationBuffer.length() > INFLATE_LESS_THAN_ROUGHLY) {
        length = 0;
        return nullptr;
    }

    if (dynamicInflationBuffer.length()) {
        dynamicInflationBuffer.append(inflationBuffer, LARGE_BUFFER_SIZE - inflationStream.avail_out);

        length = dynamicInflationBuffer.length();
        return (char *) dynamicInflationBuffer.data();
    }

    length = LARGE_BUFFER_SIZE - inflationStream.avail_out;
    return inflationBuffer;
}

void Hub::onServerAccept(uS::Socket s) {
    uS::SocketData *socketData = s.getSocketData();
    s.enterState<HttpSocket<SERVER>>(new HttpSocket<SERVER>::Data(socketData));
    ((Group<SERVER> *) socketData->nodeData)->addHttpSocket(s);
    ((Group<SERVER> *) socketData->nodeData)->httpConnectionHandler(s);
    s.setNoDelay(true);
    delete socketData;
}

void Hub::onClientConnection(uS::Socket s, bool error) {
    HttpSocket<CLIENT>::Data *httpSocketData = (HttpSocket<CLIENT>::Data *) s.getSocketData();

    if (error) {
        ((Group<CLIENT> *) httpSocketData->nodeData)->errorHandler(httpSocketData->httpUser);
        delete httpSocketData;
    } else {
        s.enterState<HttpSocket<CLIENT>>(s.getSocketData());
        HttpSocket<CLIENT>(s).upgrade(nullptr, nullptr, 0, nullptr, 0, nullptr);
    }
}

bool Hub::listen(const char *host, int port, uS::TLS::Context sslContext, int options, Group<SERVER> *eh) {
    if (!eh) {
        eh = (Group<SERVER> *) this;
    }

    if (uS::Node::listen<onServerAccept>(host, port, sslContext, options, (uS::NodeData *) eh, nullptr)) {
        eh->errorHandler(port);
        return false;
    }
    return true;
}

bool Hub::listen(int port, uS::TLS::Context sslContext, int options, Group<SERVER> *eh) {
    return listen(nullptr, port, sslContext, options, eh);
}

void Hub::connect(std::string uri, void *user, int timeoutMs, Group<CLIENT> *eh, std::string subprotocol) {
    if (!eh) {
        eh = (Group<CLIENT> *) this;
    }

    size_t offset = 0;
    std::string protocol = uri.substr(offset, uri.find("://")), hostname, portStr, path;
    if ((offset += protocol.length() + 3) < uri.length()) {
        hostname = uri.substr(offset, uri.find_first_of(":/", offset) - offset);

        offset += hostname.length();
        if (uri[offset] == ':') {
            offset++;
            portStr = uri.substr(offset, uri.find("/", offset) - offset);
        }

        offset += portStr.length();
        if (uri[offset] == '/') {
            path = uri.substr(++offset);
        }
    }

    if (hostname.length()) {
        int port = 80;
        bool secure = false;
        if (protocol == "wss") {
            port = 443;
            secure = true;
        } else if (protocol != "ws") {
            eh->errorHandler(user);
        }

        if (portStr.length()) {
            port = stoi(portStr);
        }

        uS::SocketData socketData((uS::NodeData *) eh);
        HttpSocket<CLIENT>::Data *httpSocketData = new HttpSocket<CLIENT>::Data(&socketData);

        std::string optionalSubprotocol;
        if (!subprotocol.empty()) {
            optionalSubprotocol = "Sec-WebSocket-Protocol: " + subprotocol + "\r\n";
        }
        httpSocketData->httpUser = user;
        httpSocketData->httpBuffer = "GET /" + path + " HTTP/1.1\r\n"
                                     "Upgrade: websocket\r\n"
                                     "Connection: Upgrade\r\n"
                                     "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n"
                                     "Host: " + hostname + "\r\n"
                                     + optionalSubprotocol +
                                     "Sec-WebSocket-Version: 13\r\n\r\n";

        uS::Socket s = uS::Node::connect<onClientConnection>(hostname.c_str(), port, secure, httpSocketData);
        if (s) {
            s.startTimeout<HttpSocket<CLIENT>::onEnd>(timeoutMs);
            // getGroup<CLIENT>(s)->addHttpSocket(s);
        }
    } else {
        eh->errorHandler(user);
    }
}

void Hub::upgrade(uv_os_sock_t fd, const char *secKey, SSL *ssl, const char *extensions, size_t extensionsLength, const char *subprotocol, size_t subprotocolLength, Group<SERVER> *serverGroup) {
    if (!serverGroup) {
        serverGroup = &getDefaultGroup<SERVER>();
    }

    uS::Socket s = uS::Socket::init((uS::NodeData *) serverGroup, fd, ssl);
    uS::SocketData *socketData = s.getSocketData();
    HttpSocket<SERVER>::Data *temporaryHttpData = new HttpSocket<SERVER>::Data(socketData);
    delete socketData;
    s.enterState<HttpSocket<SERVER>>(temporaryHttpData);

    bool perMessageDeflate;
    HttpSocket<SERVER>(s).upgrade(secKey, extensions, extensionsLength, subprotocol, subprotocolLength, &perMessageDeflate);
    s.enterState<WebSocket<SERVER>>(new WebSocket<SERVER>::Data(perMessageDeflate, s.getSocketData()));
    serverGroup->addWebSocket(s);
    serverGroup->connectionHandler(WebSocket<SERVER>(s), HttpRequest({}));
    delete temporaryHttpData;
}

}
