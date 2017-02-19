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
void Group<isServer>::timerCallback(uv_timer_t *timer) {
    Group<isServer> *group = (Group<isServer> *) timer->data;

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
    timer = new uv_timer_t;
    uv_timer_init(loop, timer);
    timer->data = this;
    uv_timer_start(timer, timerCallback, intervalMs, intervalMs);
    userPingMessage = userMessage;
}

// WIP
template <bool isServer>
void Group<isServer>::addHttpSocket(uv_poll_t *httpSocket) {

    // always clear last chain!
    ((uS::SocketData *) httpSocket->data)->next = nullptr;
    ((uS::SocketData *) httpSocket->data)->prev = nullptr;

    if (httpSocketHead) {
        uS::SocketData *nextData = (uS::SocketData *) httpSocketHead->data;
        nextData->prev = httpSocket;
        uS::SocketData *data = (uS::SocketData *) httpSocket->data;
        data->next = httpSocketHead;
    } else {
        httpTimer = new uv_timer_t;
        uv_timer_init(hub->getLoop(), httpTimer);
        httpTimer->data = this;
        uv_timer_start(httpTimer, [](uv_timer_t *httpTimer) {
            Group<isServer> *group = (Group<isServer> *) httpTimer->data;
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
void Group<isServer>::removeHttpSocket(uv_poll_t *httpSocket) {
    uS::SocketData *socketData = (uS::SocketData *) httpSocket->data;
    if (iterators.size()) {
        iterators.top() = socketData->next;
    }
    if (socketData->prev == socketData->next) {
        httpSocketHead = (uv_poll_t *) nullptr;

        uv_timer_stop(httpTimer);
        uv_close(httpTimer, [](uv_handle_t *handle) {
            delete (uv_timer_t *) handle;
        });

    } else {
        if (socketData->prev) {
            ((uS::SocketData *) socketData->prev->data)->next = socketData->next;
        } else {
            httpSocketHead = socketData->next;
        }
        if (socketData->next) {
            ((uS::SocketData *) socketData->next->data)->prev = socketData->prev;
        }
    }
}

template <bool isServer>
void Group<isServer>::addWebSocket(uv_poll_t *webSocket) {

    // always clear last chain!
    ((uS::SocketData *) webSocket->data)->next = nullptr;
    ((uS::SocketData *) webSocket->data)->prev = nullptr;

    if (webSocketHead) {
        uS::SocketData *nextData = (uS::SocketData *) webSocketHead->data;
        nextData->prev = webSocket;
        uS::SocketData *data = (uS::SocketData *) webSocket->data;
        data->next = webSocketHead;
    }
    webSocketHead = webSocket;
}

template <bool isServer>
void Group<isServer>::removeWebSocket(uv_poll_t *webSocket) {
    uS::SocketData *socketData = (uS::SocketData *) webSocket->data;
    if (iterators.size()) {
        iterators.top() = socketData->next;
    }
    if (socketData->prev == socketData->next) {
        webSocketHead = (uv_poll_t *) nullptr;
    } else {
        if (socketData->prev) {
            ((uS::SocketData *) socketData->prev->data)->next = socketData->next;
        } else {
            webSocketHead = socketData->next;
        }
        if (socketData->next) {
            ((uS::SocketData *) socketData->next->data)->prev = socketData->prev;
        }
    }
}

template <bool isServer>
Group<isServer>::Group(int extensionOptions, Hub *hub, uS::NodeData *nodeData) : uS::NodeData(*nodeData), hub(hub), extensionOptions(extensionOptions) {
    connectionHandler = [](WebSocket<isServer>, HttpRequest) {};
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
                uv_timer_stop(listenData->listenTimer);
                ::close(fd);

                SSL *ssl = listenData->ssl;
                if (ssl) {
                    SSL_free(ssl);
                }

                uv_close(listenData->listenTimer, [](uv_handle_t *handle) {
                    delete handle;
                });
            }
            delete listenData;
        }
    }

    if (async) {
        uv_close(async, [](uv_handle_t *h) {
            delete (uv_async_t *) h;
        });
    }
}

template <bool isServer>
void Group<isServer>::onConnection(std::function<void (WebSocket<isServer>, HttpRequest)> handler) {
    connectionHandler = handler;
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
        uv_timer_stop(timer);
        uv_close(timer, [](uv_handle_t *handle) {
            delete (uv_timer_t *) handle;
        });
    }
}

template struct Group<true>;
template struct Group<false>;

}
