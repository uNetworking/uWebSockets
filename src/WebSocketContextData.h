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
    /* Used for prepending unframed messages when using dedicated compressors */
    struct MessageMetadata {
        unsigned int length;
        OpCode opCode;
        bool compress;
        /* Undefined init of all members */
        MessageMetadata() {}
        MessageMetadata(unsigned int length, OpCode opCode, bool compress)
        : length(length), opCode(opCode), compress(compress) {}
    };

public:
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
    TopicTree topicTree;

    /* This is run once on start-up */
    void calculateIdleTimeoutCompnents(unsigned short idleTimeout) {
        unsigned short margin = 4;
        /* 4, 8 or 16 seconds margin based on idleTimeout */
        while ((int) idleTimeout - margin * 2 >= margin * 2 && margin < 16) {
            margin = (unsigned short) (margin << 1);
        }
        /* We should have no margin if not using sendPingsAutomatically */
        if (!sendPingsAutomatically) {
            margin = 0;
        }
        idleTimeoutComponents = {
            idleTimeout - margin,
            margin
        };
    }

    ~WebSocketContextData() {
        /* We must unregister any loop post handler here */
        Loop::get()->removePostHandler(this);
        Loop::get()->removePreHandler(this);
    }

    WebSocketContextData() : topicTree([this](Subscriber *s, Intersection &intersection) -> int {

        /* We could potentially be called here even if we have nothing to send, since we can
         * be the sender of every single message in this intersection. Also "fin" of a segment is not
         * guaranteed to be set, in case remaining segments are all from us.
         * Essentially, we cannot make strict assumptions here. Also, we can even come here corked,
         * since publish can call drain! */

        /* We rely on writing to regular asyncSockets */
        auto *asyncSocket = (AsyncSocket<SSL> *) s->user;

        /* If we are corked, do not uncork - otherwise if we cork in here, uncork before leaving */
        bool wasCorked = asyncSocket->isCorked();

        /* Do we even have room for potential data? */
        if (!maxBackpressure || asyncSocket->getBufferedAmount() < maxBackpressure) {

            /* Roll over all our segments */
            intersection.forSubscriber(topicTree.getSenderFor(s), [asyncSocket, this](std::pair<std::string_view, std::string_view> data, bool fin) {

                /* We have a segment that is not marked as last ("fin").
                 * Cork if not already so (purely for performance reasons). Does not touch "wasCorked". */
                if (!fin && !asyncSocket->isCorked() && asyncSocket->canCork()) {
                    asyncSocket->cork();
                }

                /* Pick uncompressed data track */
                std::string_view selectedData = data.first;

                /* Are we using compression? Fine, pick the compressed data track */
                WebSocketData *webSocketData = (WebSocketData *) asyncSocket->getAsyncSocketData();
                if (webSocketData->compressionStatus != WebSocketData::CompressionStatus::DISABLED) {

                    /* This is used for both shared and dedicated paths */
                    selectedData = data.second;

                    /* However, dedicated compression has its own path */
                    if (compression != SHARED_COMPRESSOR) {

                        WebSocket<SSL, true, int> *ws = (WebSocket<SSL, true, int> *) asyncSocket;

                        /* For performance reasons we always cork when in dedicated mode.
                         * Is this really the best? We already kind of cork things in Zlib?
                         * Right, formatting needs a cork buffer, right. Never mind. */
                        if (!ws->isCorked() && ws->canCork()) {
                            asyncSocket->cork();
                        }

                        while (selectedData.length()) {
                            /* Interpret the data like so, because this is how we shoved it in */
                            MessageMetadata mm;
                            memcpy((char *) &mm, selectedData.data(), sizeof(MessageMetadata));
                            std::string_view unframedMessage(selectedData.data() + sizeof(MessageMetadata), mm.length);

                            /* Skip this message if our backpressure is too high */
                            if (maxBackpressure && ws->getBufferedAmount() > maxBackpressure) {
                                break;
                            }

                            /* Here we perform the actual compression and framing */
                            ws->send(unframedMessage, mm.opCode, mm.compress);

                            /* Advance until empty */
                            selectedData.remove_prefix(sizeof(MessageMetadata) + mm.length);
                        }

                        /* Continue to next segment without executing below path */
                        return;
                    }
                }

                /* Common path for SHARED and DISABLED. It is an invalid assumption that we always are
                 * uncorked here, however the following (invalid) assumption is not critically wrong either way */

                /* Note: this assumes we are not corked, as corking will swallow things and fail later on */
                auto [written, failed] = asyncSocket->write(selectedData.data(), (int) selectedData.length());
                /* If we want strict check for success, we can ignore this check if corked and repeat below
                 * when uncorking - however this is too strict as we really care about PROGRESS rather than
                 * ENTIRE SUCCESS - we need minor API changes to support correct checks */
                if (!failed) {
                    if (this->resetIdleTimeoutOnSend) {
                        auto *webSocketData = (WebSocketData *) us_socket_ext(SSL, (us_socket_t *) asyncSocket);
                        webSocketData->hasTimedOut = false;
                        asyncSocket->timeout(this->idleTimeoutComponents.first);
                    }
                }
            });
        }

        /* We are done sending, for whatever reasons we ended up corked while not starting with "wasCorked",
         * we here need to uncork to restore the state we were called in */
        if (!wasCorked && asyncSocket->isCorked()) {
            /* Regarding timeout for writes; */
            auto [written, failed] = asyncSocket->uncork();
            /* Again, this check should be more like DID WE PROGRESS rather than DID WE SUCCEED ENTIRELY */
            if (!failed) {
                if (this->resetIdleTimeoutOnSend) {
                    auto *webSocketData = (WebSocketData *) us_socket_ext(SSL, (us_socket_t *) asyncSocket);
                    webSocketData->hasTimedOut = false;
                    asyncSocket->timeout(this->idleTimeoutComponents.first);
                }
            }
        }

        /* Defer a close if we now have (or already had) too much backpressure, or simply skip */
        if (maxBackpressure && closeOnBackpressureLimit && asyncSocket->getBufferedAmount() > maxBackpressure) {
            /* We must not immediately close the socket, as that could result in stack overflow,
            * iterator invalidation and other TopicTree::drain bugs. We may shutdown the reading side of the socket,
            * causing next iteration to error-close the socket from that context instead, if we want to */
            us_socket_shutdown_read(SSL, (us_socket_t *) asyncSocket);
        }

        /* Reserved, unused */
        return 0;
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

    /* Helper for topictree publish, common path from app and ws */
    bool publish(std::string_view topic, std::string_view message, OpCode opCode, bool compress, Subscriber *sender = nullptr) {
        bool didMatch = false;

        /* We frame the message right here and only pass raw bytes to the pub/subber */
        char *dst = (char *) malloc(protocol::messageFrameSize(message.size()));
        size_t dst_length = protocol::formatMessage<true>(dst, message.data(), message.length(), opCode, message.length(), false);

        /* If compression is disabled */
        if (compression == DISABLED) {
            /* Leave second field empty as nobody will ever read it */
            didMatch |= topicTree.publish(topic, {std::string_view(dst, dst_length), {}}, sender);
        } else {
            /* DEDICATED_COMPRESSOR always takes the same path as must always have MessageMetadata as head */
            if (compress || compression != SHARED_COMPRESSOR) {
                /* Shared compression mode publishes compressed, framed data */
                if (compression == SHARED_COMPRESSOR) {
                    /* Loop data holds shared compressor */
                    LoopData *loopData = (LoopData *) us_loop_ext((us_loop_t *) Loop::get());

                    /* Compress it */
                    std::string_view compressedMessage = loopData->deflationStream->deflate(loopData->zlibContext, message, true);

                    /* Frame it */
                    char *dst_compressed = (char *) malloc(protocol::messageFrameSize(compressedMessage.size()));
                    size_t dst_compressed_length = protocol::formatMessage<true>(dst_compressed, compressedMessage.data(), compressedMessage.length(), opCode, compressedMessage.length(), true);

                    /* Always publish the shortest one in any case */
                    didMatch |= topicTree.publish(topic, {std::string_view(dst, dst_length), dst_compressed_length >= dst_length ? std::string_view(dst, dst_length) : std::string_view(dst_compressed, dst_compressed_length)}, sender);

                    /* We don't care for allocation here */
                    ::free(dst_compressed);
                } else {
                    /* Dedicated compression mode publishes metadata + unframed uncompressed data */
                    char *dst_compressed = (char *) malloc(message.length() + sizeof(MessageMetadata));

                    MessageMetadata mm(
                        (unsigned int) message.length(),
                        opCode,
                        compress
                    );

                    memcpy(dst_compressed, (char *) &mm, sizeof(MessageMetadata));
                    memcpy(dst_compressed + sizeof(MessageMetadata), message.data(), message.length());

                    /* Interpretation of compressed data depends on what compressor we use */
                    didMatch |= topicTree.publish(topic, {
                        std::string_view(dst, dst_length),
                        std::string_view(dst_compressed, message.length() + sizeof(MessageMetadata))
                    }, sender);

                    ::free(dst_compressed);
                }
            } else {
                /* If not compressing, put same message on both tracks (only valid for SHARED_COMPRESSOR).
                 * DEDICATED_COMPRESSOR_xKB must never end up here as we don't put a proper head here. */
                didMatch |= topicTree.publish(topic, {std::string_view(dst, dst_length), std::string_view(dst, dst_length)}, sender);
            }
        }

        ::free(dst);

        return didMatch;
    }
};

}

#endif // UWS_WEBSOCKETCONTEXTDATA_H
