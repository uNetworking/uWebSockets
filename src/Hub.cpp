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
    s.startTimeout<HTTPSocket<SERVER>::onEnd>();
    s.enterState<HTTPSocket<SERVER>>(new HTTPSocket<SERVER>::Data(socketData));
    delete socketData;
}

void Hub::onClientConnection(uS::Socket s, bool error) {
    HTTPSocket<CLIENT>::Data *httpSocketData = (HTTPSocket<CLIENT>::Data *) s.getSocketData();

    if (error) {
        ((Group<CLIENT> *) httpSocketData->nodeData)->errorHandler(httpSocketData->httpUser);
        delete httpSocketData;
    } else {
        s.enterState<HTTPSocket<CLIENT>>(s.getSocketData());
        HTTPSocket<CLIENT>(s).upgrade();
    }
}

bool Hub::listen(int port, uS::TLS::Context sslContext, int options, Group<SERVER> *eh) {
    if (!eh) {
        eh = (Group<SERVER> *) this;
    }

    if (uS::Node::listen<onServerAccept>(port, sslContext, options, (uS::NodeData *) eh, nullptr)) {
        eh->errorHandler(port);
        return false;
    }
    return true;
}

void Hub::connect(std::string uri, void *user, int timeoutMs, Group<CLIENT> *eh) {
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
        HTTPSocket<CLIENT>::Data *httpSocketData = new HTTPSocket<CLIENT>::Data(&socketData);

        httpSocketData->host = hostname;
        httpSocketData->path = path;
        httpSocketData->httpUser = user;

        uS::Socket s = uS::Node::connect<onClientConnection>(hostname.c_str(), port, secure, httpSocketData);
        if (s) {
            s.startTimeout<HTTPSocket<CLIENT>::onEnd>(timeoutMs);
        }
    } else {
        eh->errorHandler(user);
    }
}

bool Hub::upgrade(uv_os_sock_t fd, const char *secKey, SSL *ssl, const char *extensions, size_t extensionsLength, Group<SERVER> *serverGroup) {
    if (!serverGroup) {
        serverGroup = &getDefaultGroup<SERVER>();
    }

    uS::Socket s = uS::Socket::init((uS::NodeData *) serverGroup, fd, ssl);
    uS::SocketData *socketData = s.getSocketData();
    HTTPSocket<SERVER>::Data *temporaryHttpData = new HTTPSocket<SERVER>::Data(socketData);
    delete socketData;
    s.enterState<HTTPSocket<SERVER>>(temporaryHttpData);

    // todo: move this into upgrade call
    bool perMessageDeflate = false;
    std::string extensionsResponse;
    if (extensionsLength) {
        ExtensionsNegotiator<uWS::SERVER> extensionsNegotiator(serverGroup->extensionOptions);
        extensionsNegotiator.readOffer(std::string(extensions, extensionsLength));
        extensionsResponse = extensionsNegotiator.generateOffer();
        if (extensionsNegotiator.getNegotiatedOptions() & PERMESSAGE_DEFLATE) {
            perMessageDeflate = true;
        }
    }

    if (HTTPSocket<SERVER>(s).upgrade(secKey, extensionsResponse.data(), extensionsResponse.length())) {
        s.enterState<WebSocket<SERVER>>(new WebSocket<SERVER>::Data(perMessageDeflate, s.getSocketData()));
        serverGroup->addWebSocket(s);
        serverGroup->connectionHandler(WebSocket<SERVER>(s), {nullptr, nullptr, 0, 0});
        delete temporaryHttpData;
        return true;
    }
    return false;
}

}
