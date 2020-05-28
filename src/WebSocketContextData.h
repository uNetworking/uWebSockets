/*
 * Authored by Alex Hultman, 2018-2019.
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

#include "f2/function2.hpp"
#include <string_view>

#include "WebSocketProtocol.h"
#include "TopicTree.h"
#include "WebSocketData.h"

namespace uWS {

template <bool, bool> struct WebSocket;

/* todo: this looks identical to WebSocketBehavior, why not just std::move that entire thing in? */

template <bool SSL>
struct WebSocketContextData {
    /* The callbacks for this context */
    fu2::unique_function<void(WebSocket<SSL, true> *, std::string_view, uWS::OpCode)> messageHandler = nullptr;
    fu2::unique_function<void(WebSocket<SSL, true> *)> drainHandler = nullptr;
    fu2::unique_function<void(WebSocket<SSL, true> *, int, std::string_view)> closeHandler = nullptr;
    /* Todo: these should take message also; breaking change for v0.18 */
    fu2::unique_function<void(WebSocket<SSL, true> *)> pingHandler = nullptr;
    fu2::unique_function<void(WebSocket<SSL, true> *)> pongHandler = nullptr;

    /* Settings for this context */
    size_t maxPayloadLength = 0;
    int idleTimeout = 0;

    /* There needs to be a maxBackpressure which will force close everything over that limit */
    size_t maxBackpressure = 0;

    /* Each websocket context has a topic tree for pub/sub */
    TopicTree topicTree;

    ~WebSocketContextData() {
        /* We must unregister any loop post handler here */
        Loop::get()->removePostHandler(this);
        Loop::get()->removePreHandler(this);
    }

    WebSocketContextData() : topicTree([this](Subscriber *s, std::pair<std::string_view, std::string_view> data) -> int {
        /* We rely on writing to regular asyncSockets */
        auto *asyncSocket = (AsyncSocket<SSL> *) s->user;

        /* Check if we now have too much backpressure (todo: don't buffer up before check) */
        if (!maxBackpressure || (unsigned int) asyncSocket->getBufferedAmount() < maxBackpressure) {
            /* Pick uncompressed data track */
            std::string_view selectedData = data.first;

            /* Are we using compression? Fine, pick the compressed data track */
            WebSocketData *webSocketData = (WebSocketData *) asyncSocket->getAsyncSocketData();
            if (webSocketData->compressionStatus != WebSocketData::CompressionStatus::DISABLED) {
                selectedData = data.second;
            }

            /* Note: this assumes we are not corked, as corking will swallow things and fail later on */
            auto [written, failed] = asyncSocket->write(selectedData.data(), (int) selectedData.length());
            if (!failed) {
                asyncSocket->timeout(this->idleTimeout);
            }
            
            /* Failing here must not immediately close the socket, as that could result in stack overflow,
             * iterator invalidation and other TopicTree::drain bugs. We may shutdown the reading side of the socket,
             * causing next iteration to error-close the socket from that context instead, if we want to */
        }
        
        /* If we have too much backpressure, simply skip sending from here */

        /* Reserved, unused */
        return 0;
    }) {
        /* We empty for both pre and post just to make sure */
        Loop::get()->addPostHandler(this, [this](Loop *loop) {
            /* Commit pub/sub batches every loop iteration */
            topicTree.drain();
        });

        Loop::get()->addPreHandler(this, [this](Loop *loop) {
            /* Commit pub/sub batches every loop iteration */
            topicTree.drain();
        });
    }

    /* Helper for topictree publish, common path from app and ws */
    void publish(std::string_view topic, std::string_view message, OpCode opCode, bool compress) {
        /* We frame the message right here and only pass raw bytes to the pub/subber */
        char *dst = (char *) malloc(protocol::messageFrameSize(message.size()));
        size_t dst_length = protocol::formatMessage<true>(dst, message.data(), message.length(), opCode, message.length(), false);

        if (compress) {
            /* Loop data holds shared compressor */
            LoopData *loopData = (LoopData *) us_loop_ext((us_loop_t *) Loop::get());

            /* Compress it */
            std::string_view compressedMessage = loopData->deflationStream->deflate(loopData->zlibContext, message, true);

            /* Frame it */
            char *dst_compressed = (char *) malloc(protocol::messageFrameSize(compressedMessage.size()));
            size_t dst_compressed_length = protocol::formatMessage<true>(dst_compressed, compressedMessage.data(), compressedMessage.length(), opCode, compressedMessage.length(), true);

            /* Always publish the shortest one in any case */
            topicTree.publish(topic, {std::string_view(dst, dst_length), dst_compressed_length >= dst_length ? std::string_view(dst, dst_length) : std::string_view(dst_compressed, dst_compressed_length)});

            /* We don't care for allocation here */
            ::free(dst_compressed);
        } else {
            /* If not compressing, put same message on both tracks */
            topicTree.publish(topic, {std::string_view(dst, dst_length), std::string_view(dst, dst_length)});
        }

        ::free(dst);
    }
};

}

#endif // UWS_WEBSOCKETCONTEXTDATA_H
