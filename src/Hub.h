#ifndef HUB_UWS_H
#define HUB_UWS_H

#include "Group.h"
#include "Node.h"
#include <string>
#include <zlib.h>
#include <mutex>

static_assert (UV_VERSION_MINOR >= 3, "µWebSockets requires libuv >=1.3.0");

namespace uWS {

struct WIN32_EXPORT Hub : private uS::Node, public Group<SERVER>, public Group<CLIENT> {

    template <bool isServer>
    Group<isServer> *createGroup(int extensionOptions = 0) {
        return new Group<isServer>(extensionOptions, this, nodeData);
    }

    template <bool isServer>
    Group<isServer> &getDefaultGroup() {
        return (Group<isServer> &) *this;
    }

    struct ConnectionData {
        std::string path;
        void *user;
        Group<CLIENT> *group;
    };

    z_stream inflationStream = {};
    char *inflationBuffer;
    char *inflate(char *data, size_t &length);
    std::string dynamicInflationBuffer;
    static const int LARGE_BUFFER_SIZE = 300 * 1024;

    static void onServerAccept(uS::Socket s);
    static void onClientConnection(uS::Socket s, bool error);

    bool listen(int port, uS::TLS::Context sslContext = nullptr, int options = 0, Group<SERVER> *eh = nullptr);
    bool listen(const char *host, int port, uS::TLS::Context sslContext = nullptr, int options = 0, Group<SERVER> *eh = nullptr);
    void connect(std::string uri, void *user, int timeoutMs = 5000, Group<CLIENT> *eh = nullptr, std::string subprotocol = "");
    void upgrade(uv_os_sock_t fd, const char *secKey, SSL *ssl, const char *extensions, size_t extensionsLength, const char *subprotocol, size_t subprotocolLength, Group<SERVER> *serverGroup = nullptr);

    Hub(int extensionOptions = 0, bool useDefaultLoop = false) : uS::Node(LARGE_BUFFER_SIZE, WebSocketProtocol<SERVER>::CONSUME_PRE_PADDING, WebSocketProtocol<SERVER>::CONSUME_POST_PADDING, useDefaultLoop),
                                             Group<SERVER>(extensionOptions, this, nodeData), Group<CLIENT>(0, this, nodeData) {
        inflateInit2(&inflationStream, -15);
        inflationBuffer = new char[LARGE_BUFFER_SIZE];
    }

    ~Hub() {
        inflateEnd(&inflationStream);
        delete [] inflationBuffer;
    }

    using uS::Node::run;
    using uS::Node::getLoop;
    using Group<SERVER>::onConnection;
    using Group<CLIENT>::onConnection;
    using Group<SERVER>::onMessage;
    using Group<CLIENT>::onMessage;
    using Group<SERVER>::onDisconnection;
    using Group<CLIENT>::onDisconnection;
    using Group<SERVER>::onPing;
    using Group<CLIENT>::onPing;
    using Group<SERVER>::onPong;
    using Group<CLIENT>::onPong;
    using Group<SERVER>::onError;
    using Group<CLIENT>::onError;
    using Group<SERVER>::onHttpRequest;
    using Group<SERVER>::onHttpData;
    using Group<SERVER>::onHttpConnection;
    using Group<SERVER>::onHttpDisconnection;
    using Group<SERVER>::onHttpUpgrade;
    using Group<SERVER>::onCancelledHttpRequest;
};

}

#endif // HUB_UWS_H
