#ifndef PARSER_H
#define PARSER_H

#include "SocketData.h"
#include "UTF8.h"
#include "Network.h"
#include <uv.h>

#define STRICT_WS

namespace uWS {

class Parser {
private:
    typedef uint16_t frameFormat;
    static inline bool fin(frameFormat &frame) {return frame & 128;}
    static inline unsigned char opCode(frameFormat &frame) {return frame & 15;}
    static inline unsigned char payloadLength(frameFormat &frame) {return (frame >> 8) & 127;}
    static inline bool rsv3(frameFormat &frame) {return frame & 16;}
    static inline bool rsv2(frameFormat &frame) {return frame & 32;}
    static inline bool rsv1(frameFormat &frame) {return frame & 64;}
    static inline bool mask(frameFormat &frame) {return frame & 32768;}

    static inline void unmask_imprecise(char *dst, char *src, char *mask, unsigned int length)
    {
        for (unsigned int n = (length >> 2) + 1; n; n--) {
            *(dst++) = *(src++) ^ mask[0];
            *(dst++) = *(src++) ^ mask[1];
            *(dst++) = *(src++) ^ mask[2];
            *(dst++) = *(src++) ^ mask[3];
        }
    }

    static inline void unmask_imprecise_copy_mask(char *dst, char *src, char *maskPtr, unsigned int length)
    {
        char mask[4] = {maskPtr[0], maskPtr[1], maskPtr[2], maskPtr[3]};
        unmask_imprecise(dst, src, mask, length);
    }

    static inline void rotate_mask(unsigned int offset, char *mask)
    {
        char originalMask[4] = {mask[0], mask[1], mask[2], mask[3]};
        mask[(0 + offset) % 4] = originalMask[0];
        mask[(1 + offset) % 4] = originalMask[1];
        mask[(2 + offset) % 4] = originalMask[2];
        mask[(3 + offset) % 4] = originalMask[3];
    }

    static inline void unmask_inplace(char *data, char *stop, char *mask)
    {
        while (data < stop) {
            *(data++) ^= mask[0];
            *(data++) ^= mask[1];
            *(data++) ^= mask[2];
            *(data++) ^= mask[3];
        }
    }

    template <typename T>
    static inline void consumeIncompleteMessage(int length, const int headerLength, T fullPayloadLength, SocketData *socketData, char *src, uv_poll_t *p)
    {
        socketData->spillLength = 0;
        socketData->state = READ_MESSAGE;
        socketData->remainingBytes = fullPayloadLength - length + headerLength;

        memcpy(socketData->mask, src + headerLength - 4, 4);
        unmask_imprecise(src, src + headerLength, socketData->mask, length);
        rotate_mask(4 - (length - headerLength) % 4, socketData->mask);

        WebSocket(p).handleFragment(src, length - headerLength,
                                    socketData->opCode[(unsigned char) socketData->opStack], socketData->fin, socketData->remainingBytes, socketData->pmd && socketData->pmd->compressedFrame);
    }

    template <typename T>
    static inline int consumeCompleteMessage(int &length, const int headerLength, T fullPayloadLength, SocketData *socketData, char **src, frameFormat &frame, uv_poll_t *p)
    {
        unmask_imprecise_copy_mask(*src, *src + headerLength, *src + headerLength - 4, fullPayloadLength);
        WebSocket(p).handleFragment(*src, fullPayloadLength, socketData->opCode[(unsigned char) socketData->opStack], socketData->fin, 0, socketData->pmd && socketData->pmd->compressedFrame);

        if (uv_is_closing((uv_handle_t *) p) || socketData->state == CLOSING) {
            return 1;
        }

        if (fin(frame)) {
            socketData->opStack--;
        }

        *src += fullPayloadLength + headerLength;
        length -= fullPayloadLength + headerLength;
        socketData->spillLength = 0;
        return 0;
    }

    static inline void consumeEntireBuffer(char *src, int length, SocketData *socketData, uv_poll_t *p)
    {
        int n = (length >> 2) + bool(length % 4); // this should always overwrite!

        unmask_inplace(src, src + n * 4, socketData->mask);
        socketData->remainingBytes -= length;
        WebSocket(p).handleFragment((const char *) src, length,
                                    socketData->opCode[(unsigned char) socketData->opStack], socketData->fin, socketData->remainingBytes, socketData->pmd && socketData->pmd->compressedFrame);

        if (uv_is_closing((uv_handle_t *) p) || socketData->state == CLOSING) {
            return;
        }

        // if we perfectly read the last of the message, change state!
        if (!socketData->remainingBytes) {
            socketData->state = READ_HEAD;

            if (socketData->fin) {
                socketData->opStack--;
            }
        } else if (length % 4) {
            rotate_mask(4 - (length % 4), socketData->mask);
        }
    }

    static inline int consumeCompleteTail(char **src, int &length, SocketData *socketData, uv_poll_t *p)
    {
        int n = (socketData->remainingBytes >> 2);
        unmask_inplace(*src, *src + n * 4, socketData->mask);
        for (int i = 0, s = socketData->remainingBytes % 4; i < s; i++) {
            (*src)[n * 4 + i] ^= socketData->mask[i];
        }

        WebSocket(p).handleFragment((const char *) *src, socketData->remainingBytes,
                                    socketData->opCode[(unsigned char) socketData->opStack], socketData->fin, 0, socketData->pmd && socketData->pmd->compressedFrame);

        if (uv_is_closing((uv_handle_t *) p) || socketData->state == CLOSING) {
            return 1;
        }

        if (socketData->fin) {
            socketData->opStack--;
        }

        (*src) += socketData->remainingBytes;
        length -= socketData->remainingBytes;

        socketData->state = READ_HEAD;
        return 0;
    }

public:
    // LONG_MESSAGE_HEADER + 4 byte mask
    static const int CONSUME_POST_PADDING = 18;

    static inline void consume(int length, char *src, SocketData *socketData, uv_poll_t *p)
    {
        parseNext:
        if (socketData->state == READ_HEAD) {
            while (length >= (int) sizeof(frameFormat)) {
                frameFormat frame = *(frameFormat *) src;

                int lastFin = socketData->fin;
                socketData->fin = fin(frame);

                if (socketData->pmd && opCode(frame) != 0) {
                    socketData->pmd->compressedFrame = rsv1(frame);
                }

    #ifdef STRICT_WS
                // invalid reserved bits
                if ((rsv1(frame) && !socketData->pmd) || rsv2(frame) || rsv3(frame)) {
                    WebSocket(p).close(true, 1006);
                    return;
                }

                // invalid opcodes
                if ((opCode(frame) > 2 && opCode(frame) < 8) || opCode(frame) > 10) {
                    WebSocket(p).close(true, 1006);
                    return;
                }
    #endif

                // do not store opCode continuation!
                if (opCode(frame)) {

                    // if empty stack or a new op-code, push on stack!
                    if (socketData->opStack == -1 || socketData->opCode[(unsigned char) socketData->opStack] != (OpCode) opCode(frame)) {
                        socketData->opCode[(unsigned char) ++socketData->opStack] = (OpCode) opCode(frame);
                    }

    #ifdef STRICT_WS
                    // Case 5.18
                    if (socketData->opStack == 0 && !lastFin && fin(frame)) {
                        WebSocket(p).close(true, 1006);
                        return;
                    }

                    // control frames cannot be fragmented or long
                    if (opCode(frame) > 2 && (!fin(frame) || payloadLength(frame) > 125)) {
                        WebSocket(p).close(true, 1006);
                        return;
                    }

                } else {
                    // continuation frame must have a opcode prior!
                    if (socketData->opStack == -1) {
                        WebSocket(p).close(true, 1006);
                        return;
                    }
    #endif
                }

                if (payloadLength(frame) > 125) {
                    if (payloadLength(frame) == 126) {
                        const int MEDIUM_MESSAGE_HEADER = 8;
                        // we need to have enough length to read the long length
                        if (length < 2 + (int) sizeof(uint16_t)) {
                            break;
                        }
                        if (ntohs(*(uint16_t *) &src[2]) <= length - MEDIUM_MESSAGE_HEADER) {
                            if (consumeCompleteMessage(length, MEDIUM_MESSAGE_HEADER, ntohs(*(uint16_t *) &src[2]), socketData, &src, frame, p)) {
                                return;
                            }
                        } else {
                            if (length < MEDIUM_MESSAGE_HEADER + 1) {
                                break;
                            }
                            consumeIncompleteMessage(length, MEDIUM_MESSAGE_HEADER, ntohs(*(uint16_t *) &src[2]), socketData, src, p);
                            return;
                        }
                    } else {
                        const int LONG_MESSAGE_HEADER = 14;
                        // we need to have enough length to read the long length
                        if (length < 2 + (int) sizeof(uint64_t)) {
                            break;
                        }
                        if (be64toh(*(uint64_t *) &src[2]) <= (uint64_t) length - LONG_MESSAGE_HEADER) {
                            if (consumeCompleteMessage(length, LONG_MESSAGE_HEADER, be64toh(*(uint64_t *) &src[2]), socketData, &src, frame, p)) {
                                return;
                            }
                        } else {
                            if (length < LONG_MESSAGE_HEADER + 1) {
                                break;
                            }
                            consumeIncompleteMessage(length, LONG_MESSAGE_HEADER, be64toh(*(uint64_t *) &src[2]), socketData, src, p);
                            return;
                        }
                    }
                } else {
                    const int SHORT_MESSAGE_HEADER = 6;
                    if (payloadLength(frame) <= length - SHORT_MESSAGE_HEADER) {
                        if (consumeCompleteMessage(length, SHORT_MESSAGE_HEADER, payloadLength(frame), socketData, &src, frame, p)) {
                            return;
                        }
                    } else {
                        if (length < SHORT_MESSAGE_HEADER + 1) {
                            break;
                        }
                        consumeIncompleteMessage(length, SHORT_MESSAGE_HEADER, payloadLength(frame), socketData, src, p);
                        return;
                    }
                }
            }
            if (length) {
                memcpy(socketData->spill, src, length);
                socketData->spillLength = length;
            }
        } else {
            if (socketData->remainingBytes < (unsigned int) length) {
                if (consumeCompleteTail(&src, length, socketData, p)) {
                    return;
                }
                goto parseNext;
            } else {
                consumeEntireBuffer(src, length, socketData, p);
            }
        }
    }

    static std::tuple<unsigned short, char *, size_t> parseCloseFrame(std::string &payload)
    {
        unsigned short code = 0;
        char *message = nullptr;
        size_t length = 0;

        if (payload.length() >= 2) {
            code = ntohs(*(uint16_t *) payload.data());

            // correct bad codes
            if (code < 1000 || code > 1011 || (code >= 1004 && code <= 1006)) {
                code = 0;
            }
        }

        if (payload.length() > 2) {
            message = (char *) payload.data() + 2;
            length = payload.length() - 2;

            // check utf-8
            if (!isValidUtf8((unsigned char *) message, length)) {
                code = length = 0;
            }
        }

        return std::make_tuple(code, message, length);
    }
};

}

#endif // PARSER_H
