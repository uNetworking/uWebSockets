#include "Group.h"
#include "Hub.h"

namespace uWS {

template <bool isServer>
void Group<isServer>::setUserData(void *user) {
    this->userData = user;
}

template <bool isServer>
void *Group<isServer>::getUserData() {
    return userData;
}

template <bool isServer>
void Group<isServer>::timerCallback(Timer *timer) {
    Group<isServer> *group = (Group<isServer> *) timer->getData();

    group->forEach([](uWS::WebSocket<isServer> ws) {
        typename uWS::WebSocket<isServer>::Data *webSocketData = (typename uWS::WebSocket<isServer>::Data *) ws.getSocketData();
        if (webSocketData->hasOutstandingPong) {
            ws.terminate();
        } else {
            webSocketData->hasOutstandingPong = true;
        }
    });

    if (group->userPingMessage.length()) {
        group->broadcast(group->userPingMessage.data(), group->userPingMessage.length(), OpCode::TEXT);
    } else {
        group->broadcast(nullptr, 0, OpCode::PING);
    }
}

template <bool isServer>
void Group<isServer>::startAutoPing(int intervalMs, std::string userMessage) {
    timer = new Timer(loop);
    timer->setData(this);
    timer->start(timerCallback, intervalMs, intervalMs);
    userPingMessage = userMessage;
}

// WIP
template <bool isServer>
void Group<isServer>::addHttpSocket(Poll *httpSocket) {

    // always clear last chain!
    ((uS::SocketData *) httpSocket->getData())->next = nullptr;
    ((uS::SocketData *) httpSocket->getData())->prev = nullptr;

    if (httpSocketHead) {
        uS::SocketData *nextData = (uS::SocketData *) httpSocketHead->getData();
        nextData->prev = httpSocket;
        uS::SocketData *data = (uS::SocketData *) httpSocket->getData();
        data->next = httpSocketHead;
    } else {
        httpTimer = new Timer(hub->getLoop());
        httpTimer->setData(this);
        httpTimer->start([](Timer *httpTimer) {
            Group<isServer> *group = (Group<isServer> *) httpTimer->getData();
            group->forEachHttpSocket([](HttpSocket<isServer> httpSocket) {
                if (httpSocket.getData()->missedDeadline) {
                    // recursive? don't think so!
                    httpSocket.terminate();
                } else if (!httpSocket.getData()->outstandingResponsesHead) {
                    httpSocket.getData()->missedDeadline = true;
                }
            });
        }, 1000, 1000);
    }
    httpSocketHead = httpSocket;
}

// WIP
template <bool isServer>
void Group<isServer>::removeHttpSocket(Poll *httpSocket) {
    uS::SocketData *socketData = (uS::SocketData *) httpSocket->getData();
    if (iterators.size()) {
        iterators.top() = socketData->next;
    }
    if (socketData->prev == socketData->next) {
        httpSocketHead = (Poll *) nullptr;

        httpTimer->stop();
        httpTimer->close();

    } else {
        if (socketData->prev) {
            ((uS::SocketData *) socketData->prev->getData())->next = socketData->next;
        } else {
            httpSocketHead = socketData->next;
        }
        if (socketData->next) {
            ((uS::SocketData *) socketData->next->getData())->prev = socketData->prev;
        }
    }
}

template <bool isServer>
void Group<isServer>::addWebSocket(Poll *webSocket) {

    // always clear last chain!
    ((uS::SocketData *) webSocket->getData())->next = nullptr;
    ((uS::SocketData *) webSocket->getData())->prev = nullptr;

    if (webSocketHead) {
        uS::SocketData *nextData = (uS::SocketData *) webSocketHead->getData();
        nextData->prev = webSocket;
        uS::SocketData *data = (uS::SocketData *) webSocket->getData();
        data->next = webSocketHead;
    }
    webSocketHead = webSocket;
}

template <bool isServer>
void Group<isServer>::removeWebSocket(Poll *webSocket) {
    uS::SocketData *socketData = (uS::SocketData *) webSocket->getData();
    if (iterators.size()) {
        iterators.top() = socketData->next;
    }
    if (socketData->prev == socketData->next) {
        webSocketHead = (Poll *) nullptr;
    } else {
        if (socketData->prev) {
            ((uS::SocketData *) socketData->prev->getData())->next = socketData->next;
        } else {
            webSocketHead = socketData->next;
        }
        if (socketData->next) {
            ((uS::SocketData *) socketData->next->getData())->prev = socketData->prev;
        }
    }
}

template <bool isServer>
Group<isServer>::Group(int extensionOptions, Hub *hub, uS::NodeData *nodeData) : uS::NodeData(*nodeData), hub(hub), extensionOptions(extensionOptions) {
    connectionHandler = [](WebSocket<isServer>, HttpRequest) {};
    transferHandler = [](WebSocket<isServer>) {};
    messageHandler = [](WebSocket<isServer>, char *, size_t, OpCode) {};
    disconnectionHandler = [](WebSocket<isServer>, int, char *, size_t) {};
    pingHandler = pongHandler = [](WebSocket<isServer>, char *, size_t) {};
    errorHandler = [](errorType) {};
    httpRequestHandler = [](HttpResponse *, HttpRequest, char *, size_t, size_t) {};
    httpConnectionHandler = [](HttpSocket<isServer>) {};
    httpDisconnectionHandler = [](HttpSocket<isServer>) {};
    httpCancelledRequestHandler = [](HttpResponse *) {};
    httpDataHandler = [](HttpResponse *, char *, size_t, size_t) {};

    this->extensionOptions |= CLIENT_NO_CONTEXT_TAKEOVER | SERVER_NO_CONTEXT_TAKEOVER;
}

template <bool isServer>
void Group<isServer>::stopListening() {
    if (isServer) {
        uS::ListenData *listenData = (uS::ListenData *) user;
        if (listenData) {
            if (listenData->listenPoll)
                uS::Socket(listenData->listenPoll).close();
            else if (listenData->listenTimer) {
                uv_os_sock_t fd = listenData->sock;
                listenData->listenTimer->stop();
                ::close(fd);

                SSL *ssl = listenData->ssl;
                if (ssl) {
                    SSL_free(ssl);
                }

                listenData->listenTimer->close();
            }
            delete listenData;
        }
    }

    if (async) {
        async->close();
    }
}

template <bool isServer>
void Group<isServer>::onConnection(std::function<void (WebSocket<isServer>, HttpRequest)> handler) {
    connectionHandler = handler;
}

template <bool isServer>
void Group<isServer>::onTransfer(std::function<void (WebSocket<isServer>)> handler) {
    transferHandler = handler;
}

template <bool isServer>
void Group<isServer>::onMessage(std::function<void (WebSocket<isServer>, char *, size_t, OpCode)> handler) {
    messageHandler = handler;
}

template <bool isServer>
void Group<isServer>::onDisconnection(std::function<void (WebSocket<isServer>, int, char *, size_t)> handler) {
    disconnectionHandler = handler;
}

template <bool isServer>
void Group<isServer>::onPing(std::function<void (WebSocket<isServer>, char *, size_t)> handler) {
    pingHandler = handler;
}

template <bool isServer>
void Group<isServer>::onPong(std::function<void (WebSocket<isServer>, char *, size_t)> handler) {
    pongHandler = handler;
}

template <bool isServer>
void Group<isServer>::onError(std::function<void (typename Group::errorType)> handler) {
    errorHandler = handler;
}

template <bool isServer>
void Group<isServer>::onHttpConnection(std::function<void (HttpSocket<isServer>)> handler) {
    httpConnectionHandler = handler;
}

template <bool isServer>
void Group<isServer>::onHttpRequest(std::function<void (HttpResponse *, HttpRequest, char *, size_t, size_t)> handler) {
    httpRequestHandler = handler;
}

template <bool isServer>
void Group<isServer>::onHttpData(std::function<void(HttpResponse *, char *, size_t, size_t)> handler) {
    httpDataHandler = handler;
}

template <bool isServer>
void Group<isServer>::onHttpDisconnection(std::function<void (HttpSocket<isServer>)> handler) {
    httpDisconnectionHandler = handler;
}

template <bool isServer>
void Group<isServer>::onCancelledHttpRequest(std::function<void (HttpResponse *)> handler) {
    httpCancelledRequestHandler = handler;
}

template <bool isServer>
void Group<isServer>::onHttpUpgrade(std::function<void(HttpSocket<isServer>, HttpRequest)> handler) {
    httpUpgradeHandler = handler;
}

template <bool isServer>
void Group<isServer>::broadcast(const char *message, size_t length, OpCode opCode) {
    typename WebSocket<isServer>::PreparedMessage *preparedMessage = WebSocket<isServer>::prepareMessage((char *) message, length, opCode, false);
    forEach([preparedMessage](uWS::WebSocket<isServer> ws) {
        ws.sendPrepared(preparedMessage);
    });
    WebSocket<isServer>::finalizeMessage(preparedMessage);
}

template <bool isServer>
void Group<isServer>::terminate() {
    forEach([](uWS::WebSocket<isServer> ws) {
        ws.terminate();
    });
    stopListening();
}

template <bool isServer>
void Group<isServer>::close(int code, char *message, size_t length) {
    forEach([code, message, length](uWS::WebSocket<isServer> ws) {
        ws.close(code, message, length);
    });
    stopListening();
    if (timer) {
        timer->stop();
        timer->close();
    }
}

template struct Group<true>;
template struct Group<false>;

}
