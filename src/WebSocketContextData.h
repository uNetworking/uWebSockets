/*
 * Authored by Alex Hultman, 2018-2020.
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

#ifndef UWS_WEBSOCKETCONTEXTDATA_H
#define UWS_WEBSOCKETCONTEXTDATA_H

#include "Loop.h"
#include "AsyncSocket.h"

#include "MoveOnlyFunction.h"
#include <string_view>
#include <vector>

#include "WebSocketProtocol.h"
#include "TopicTree.h"
#include "WebSocketData.h"

namespace uWS {

template <bool, bool, typename> struct WebSocket;

/* todo: this looks identical to WebSocketBehavior, why not just std::move that entire thing in? */

template <bool SSL, typename USERDATA>
struct WebSocketContextData {
private:

public:
    /* Type queued up when publishing */
    struct TopicTreeMessage {
        std::string message;
        OpCode opCode;
        bool compress;
    };

    /* All WebSocketContextData holds a list to all other WebSocketContextData in this app.
     * We cannot type it USERDATA since different WebSocketContextData can have different USERDATA. */
    std::vector<WebSocketContextData<SSL, int> *> adjacentWebSocketContextDatas;

    /* The callbacks for this context */
    MoveOnlyFunction<void(WebSocket<SSL, true, USERDATA> *)> openHandler = nullptr;
    MoveOnlyFunction<void(WebSocket<SSL, true, USERDATA> *, std::string_view, OpCode)> messageHandler = nullptr;
    MoveOnlyFunction<void(WebSocket<SSL, true, USERDATA> *)> drainHandler = nullptr;
    MoveOnlyFunction<void(WebSocket<SSL, true, USERDATA> *, int, std::string_view)> closeHandler = nullptr;
    /* Todo: these should take message also; breaking change for v0.18 */
    MoveOnlyFunction<void(WebSocket<SSL, true, USERDATA> *, std::string_view)> pingHandler = nullptr;
    MoveOnlyFunction<void(WebSocket<SSL, true, USERDATA> *, std::string_view)> pongHandler = nullptr;

    /* Settings for this context */
    size_t maxPayloadLength = 0;

    /* We do need these for async upgrade */
    CompressOptions compression;

    /* There needs to be a maxBackpressure which will force close everything over that limit */
    size_t maxBackpressure = 0;
    bool closeOnBackpressureLimit;
    bool resetIdleTimeoutOnSend;
    bool sendPingsAutomatically;

    /* These are calculated on creation */
    std::pair<unsigned short, unsigned short> idleTimeoutComponents;

    /* Each websocket context has a topic tree for pub/sub */
    TopicTree<TopicTreeMessage> topicTree;

    /* This is run once on start-up */
    void calculateIdleTimeoutCompnents(unsigned short idleTimeout) {
        unsigned short margin = 4;
        /* 4, 8 or 16 seconds margin based on idleTimeout */
        while ((int) idleTimeout - margin * 2 >= margin * 2 && margin < 16) {
            margin = (unsigned short) (margin << 1);
        }
        idleTimeoutComponents = {
            idleTimeout - (sendPingsAutomatically ? margin : 0), /* reduce normal idleTimeout if it is extended by ping-timeout */
            margin /* ping-timeout - also used for end() timeout */
        };
    }

    ~WebSocketContextData() {
        /* We must unregister any loop post handler here */
        Loop::get()->removePostHandler(this);
        Loop::get()->removePreHandler(this);
    }

    WebSocketContextData() : topicTree([](Subscriber *s, TopicTreeMessage &message, auto flags) {
        /* Subscriber's user is the socket */
        auto *ws = (WebSocket<SSL, true, USERDATA> *) s->user;

        /* If this is the first message we try and cork */
        bool needsUncork = false;
        if (flags & TopicTree<TopicTreeMessage>::IteratorFlags::FIRST) {
            if (ws->canCork() && !ws->isCorked()) {
                ((AsyncSocket<SSL> *)ws)->cork();
                needsUncork = true;
            }
        }

        /* If we ever overstep maxBackpresure, exit immediately */
        if (WebSocket<SSL, true, USERDATA>::SendStatus::DROPPED == ws->send(message.message, message.opCode, message.compress)) {

            if (needsUncork) {
                ((AsyncSocket<SSL> *)ws)->uncork();
            }
            /* Stop draining */
            return true;
        }

        /* If this is the last message we uncork if we are corked */
        if (flags & TopicTree<TopicTreeMessage>::IteratorFlags::LAST) {
            /* We should not uncork in all cases? */
            if (needsUncork) {
                ((AsyncSocket<SSL> *)ws)->uncork();
            }
        }

        /* Success */
        return false;
    }) {
        /* We empty for both pre and post just to make sure */
        Loop::get()->addPostHandler(this, [this](Loop */*loop*/) {
            /* Commit pub/sub batches every loop iteration */
            topicTree.drain();
        });

        Loop::get()->addPreHandler(this, [this](Loop */*loop*/) {
            /* Commit pub/sub batches every loop iteration */
            topicTree.drain();
        });
    }
};

}

#endif // UWS_WEBSOCKETCONTEXTDATA_H
