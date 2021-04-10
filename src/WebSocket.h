/*
 * Authored by Alex Hultman, 2018-2021.
 * Intellectual property of third-party.

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 *     http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef UWS_WEBSOCKET_H
#define UWS_WEBSOCKET_H

#include "WebSocketData.h"
#include "WebSocketProtocol.h"
#include "AsyncSocket.h"
#include "WebSocketContextData.h"

#include <string_view>

namespace uWS {

template <bool SSL, bool isServer, typename USERDATA>
struct WebSocket : AsyncSocket<SSL> {
    template <bool> friend struct TemplatedApp;
    template <bool> friend struct HttpResponse;
private:
    typedef AsyncSocket<SSL> Super;

    void *init(bool perMessageDeflate, CompressOptions compressOptions, std::string &&backpressure) {
        new (us_socket_ext(SSL, (us_socket_t *) this)) WebSocketData(perMessageDeflate, compressOptions, std::move(backpressure));
        return this;
    }
public:

    /* Returns pointer to the per socket user data */
    USERDATA *getUserData() {
        WebSocketData *webSocketData = (WebSocketData *) us_socket_ext(SSL, (us_socket_t *) this);
        /* We just have it overallocated by sizeof type */
        return (USERDATA *) (webSocketData + 1);
    }

    /* See AsyncSocket */
    using Super::getBufferedAmount;
    using Super::getRemoteAddress;
    using Super::getRemoteAddressAsText;
    using Super::getNativeHandle;

    /* Simple, immediate close of the socket. Emits close event */
    using Super::close;

    enum SendStatus : int {
        BACKPRESSURE,
        SUCCESS,
        DROPPED
    };

    /* Send or buffer a WebSocket frame, compressed or not. Returns BACKPRESSURE on increased user space backpressure,
     * DROPPED on dropped message (due to backpressure) or SUCCCESS if you are free to send even more now. */
    SendStatus send(std::string_view message, OpCode opCode = OpCode::BINARY, bool compress = false) {
        WebSocketContextData<SSL, USERDATA> *webSocketContextData = (WebSocketContextData<SSL, USERDATA> *) us_socket_context_ext(SSL,
            (us_socket_context_t *) us_socket_context(SSL, (us_socket_t *) this)
        );

        /* Skip sending and report success if we are over the limit of maxBackpressure */
        if (webSocketContextData->maxBackpressure && webSocketContextData->maxBackpressure < getBufferedAmount()) {
            /* Also defer a close if we should */
            if (webSocketContextData->closeOnBackpressureLimit) {
                us_socket_shutdown_read(SSL, (us_socket_t *) this);
            }
            return DROPPED;
        }

        /* Transform the message to compressed domain if requested */
        if (compress) {
            WebSocketData *webSocketData = (WebSocketData *) Super::getAsyncSocketData();

            /* Check and correct the compress hint. It is never valid to compress 0 bytes */
            if (message.length() && opCode < 3 && webSocketData->compressionStatus == WebSocketData::ENABLED) {
                LoopData *loopData = Super::getLoopData();
                /* Compress using either shared or dedicated deflationStream */
                if (webSocketData->deflationStream) {
                    message = webSocketData->deflationStream->deflate(loopData->zlibContext, message, false);
                } else {
                    message = loopData->deflationStream->deflate(loopData->zlibContext, message, true);
                }
            } else {
                compress = false;
            }
        }

        /* Check to see if we can cork for the user */
        bool automaticallyCorked = false;
        if (!Super::isCorked() && Super::canCork()) {
            automaticallyCorked = true;
            Super::cork();
        }

        /* Get size, alloate size, write if needed */
        size_t messageFrameSize = protocol::messageFrameSize(message.length());
        auto [sendBuffer, requiresWrite] = Super::getSendBuffer(messageFrameSize);
        protocol::formatMessage<isServer>(sendBuffer, message.data(), message.length(), opCode, message.length(), compress);
        /* This is the slow path, when we couldn't cork for the user */
        if (requiresWrite) {
            auto[written, failed] = Super::write(sendBuffer, (int) messageFrameSize);

            /* For now, we are slow here */
            free(sendBuffer);

            if (failed) {
                /* Return false for failure, skipping to reset the timeout below */
                return BACKPRESSURE;
            }
        }

        /* Uncork here if we automatically corked for the user */
        if (automaticallyCorked) {
            auto [written, failed] = Super::uncork();
            if (failed) {
                return BACKPRESSURE;
            }
        }

        /* Every successful send resets the timeout */
        if (webSocketContextData->resetIdleTimeoutOnSend) {
            Super::timeout(webSocketContextData->idleTimeoutComponents.first);
            WebSocketData *webSocketData = (WebSocketData *) Super::getAsyncSocketData();
            webSocketData->hasTimedOut = false;
        }

        /* Return success */
        return SUCCESS;
    }

    /* Send websocket close frame, emit close event, send FIN if successful.
     * Will not append a close reason if code is 0 or 1005. */
    void end(int code = 0, std::string_view message = {}) {
        /* Check if we already called this one */
        WebSocketData *webSocketData = (WebSocketData *) us_socket_ext(SSL, (us_socket_t *) this);
        if (webSocketData->isShuttingDown) {
            return;
        }

        /* We postpone any FIN sending to either drainage or uncorking */
        webSocketData->isShuttingDown = true;

        /* Format and send the close frame */
        static const int MAX_CLOSE_PAYLOAD = 123;
        size_t length = std::min<size_t>(MAX_CLOSE_PAYLOAD, message.length());
        char closePayload[MAX_CLOSE_PAYLOAD + 2];
        size_t closePayloadLength = protocol::formatClosePayload(closePayload, (uint16_t) code, message.data(), length);
        bool ok = send(std::string_view(closePayload, closePayloadLength), OpCode::CLOSE);

        /* FIN if we are ok and not corked */
        if (!this->isCorked()) {
            if (ok) {
                /* If we are not corked, and we just sent off everything, we need to FIN right here.
                 * In all other cases, we need to fin either if uncork was successful, or when drainage is complete. */
                this->shutdown();
            }
        }

        /* Emit close event */
        WebSocketContextData<SSL, USERDATA> *webSocketContextData = (WebSocketContextData<SSL, USERDATA> *) us_socket_context_ext(SSL,
            (us_socket_context_t *) us_socket_context(SSL, (us_socket_t *) this)
        );
        if (webSocketContextData->closeHandler) {
            webSocketContextData->closeHandler(this, code, message);
        }

        /* Make sure to unsubscribe from any pub/sub node at exit */
        webSocketContextData->topicTree.unsubscribeAll(webSocketData->subscriber, false);
        delete webSocketData->subscriber;
        webSocketData->subscriber = nullptr;
    }

    /* Corks the response if possible. Leaves already corked socket be. */
    void cork(MoveOnlyFunction<void()> &&handler) {
        if (!Super::isCorked() && Super::canCork()) {
            Super::cork();
            handler();

            /* There is no timeout when failing to uncork for WebSockets,
             * as that is handled by idleTimeout */
            auto [written, failed] = Super::uncork();
        } else {
            /* We are already corked, or can't cork so let's just call the handler */
            handler();
        }
    }

    /* Subscribe to a topic according to MQTT rules and syntax. Returns success */
    /*std::pair<unsigned int, bool>*/ bool subscribe(std::string_view topic, bool nonStrict = false) {
        WebSocketContextData<SSL, USERDATA> *webSocketContextData = (WebSocketContextData<SSL, USERDATA> *) us_socket_context_ext(SSL,
            (us_socket_context_t *) us_socket_context(SSL, (us_socket_t *) this)
        );

        /* Make us a subscriber if we aren't yet */
        WebSocketData *webSocketData = (WebSocketData *) us_socket_ext(SSL, (us_socket_t *) this);
        if (!webSocketData->subscriber) {
            webSocketData->subscriber = new Subscriber(this);
        }

        /* Cannot return numSubscribers as this is only for this particular websocket context */
        return webSocketContextData->topicTree.subscribe(topic, webSocketData->subscriber, nonStrict).second;
    }

    /* Unsubscribe from a topic, returns true if we were subscribed. */
    /*std::pair<unsigned int, bool>*/ bool unsubscribe(std::string_view topic, bool nonStrict = false) {
        WebSocketContextData<SSL, USERDATA> *webSocketContextData = (WebSocketContextData<SSL, USERDATA> *) us_socket_context_ext(SSL,
            (us_socket_context_t *) us_socket_context(SSL, (us_socket_t *) this)
        );

        WebSocketData *webSocketData = (WebSocketData *) us_socket_ext(SSL, (us_socket_t *) this);

        /* Cannot return numSubscribers as this is only for this particular websocket context */
        return webSocketContextData->topicTree.unsubscribe(topic, webSocketData->subscriber, nonStrict).second;
    }

    /* Returns whether this socket is subscribed to the specified topic */
    bool isSubscribed(std::string_view topic) {
        WebSocketContextData<SSL, USERDATA> *webSocketContextData = (WebSocketContextData<SSL, USERDATA> *) us_socket_context_ext(SSL,
            (us_socket_context_t *) us_socket_context(SSL, (us_socket_t *) this)
        );

        WebSocketData *webSocketData = (WebSocketData *) us_socket_ext(SSL, (us_socket_t *) this);
        if (!webSocketData->subscriber) {
            return false;
        }

        Topic *t = webSocketContextData->topicTree.lookupTopic(topic);
        if (t) {
            return t->subs.find(webSocketData->subscriber) != t->subs.end();
        }

        return false;
    }

    /* Iterates all topics of this WebSocket. Every topic is represented by its full name.
     * Can be called in close handler. It is possible to modify the subscription list while
     * inside the callback ONLY IF not modifying the topic passed to the callback.
     * Topic names are valid only for the duration of the callback. */
    void iterateTopics(MoveOnlyFunction<void(std::string_view/*, unsigned int*/)> cb) {
        WebSocketData *webSocketData = (WebSocketData *) us_socket_ext(SSL, (us_socket_t *) this);

        if (webSocketData->subscriber) {
            for (Topic *t : webSocketData->subscriber->subscriptions) {
                /* Lock this topic so that nobody may unsubscribe from it during this callback */
                t->locked = true;

                cb(t->fullName/*, (unsigned int) t->subs.size()*/);

                t->locked = false;
            }
        }
    }

    /* Publish a message to a topic according to MQTT rules and syntax. Returns success.
     * We, the WebSocket, must be subscribed to the topic itself and if so - no message will be sent to ourselves.
     * Use App::publish for an unconditional publish that simply publishes to whomever might be subscribed. */
    bool publish(std::string_view topic, std::string_view message, OpCode opCode = OpCode::TEXT, bool compress = false) {
        WebSocketContextData<SSL, USERDATA> *webSocketContextData = (WebSocketContextData<SSL, USERDATA> *) us_socket_context_ext(SSL,
            (us_socket_context_t *) us_socket_context(SSL, (us_socket_t *) this)
        );

        /* We cannot be a subscriber of this topic if we are not a subscriber of anything */
        WebSocketData *webSocketData = (WebSocketData *) us_socket_ext(SSL, (us_socket_t *) this);
        if (!webSocketData->subscriber) {
            /* Failure, but still do return the number of subscribers */
            return false;
        }

        /* Publish as sender, does not receive its own messages even if subscribed to relevant topics */
        bool success = webSocketContextData->publish(topic, message, opCode, compress, webSocketData->subscriber);

        /* Loop over all websocket contexts for this App */
        if (success) {
            /* Success is really only determined by the first publish. We must be subscribed to the topic. */
            for (auto *adjacentWebSocketContextData : webSocketContextData->adjacentWebSocketContextDatas) {
                adjacentWebSocketContextData->publish(topic, message, opCode, compress);
            }
        }

        return success;
    }
};

}

#endif // UWS_WEBSOCKET_H
