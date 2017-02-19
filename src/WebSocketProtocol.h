#ifndef WEBSOCKETPROTOCOL_UWS_H
#define WEBSOCKETPROTOCOL_UWS_H

// we do need to include this for htobe64, should be moved from networking!
#include "Networking.h"

#include <cstring>
#include <cstdlib>

namespace uWS {

enum OpCode : unsigned char {
    TEXT = 1,
    BINARY = 2,
    CLOSE = 8,
    PING = 9,
    PONG = 10
};

enum {
    CLIENT,
    SERVER
};

template <const bool isServer>
class WebSocketProtocol {
public:
    static const int SHORT_MESSAGE_HEADER = isServer ? 6 : 2;
    static const int MEDIUM_MESSAGE_HEADER = isServer ? 8 : 4;
    static const int LONG_MESSAGE_HEADER = isServer ? 14 : 10;

private:
    typedef uint16_t frameFormat;
    static inline bool isFin(frameFormat &frame) {return frame & 128;}
    static inline unsigned char getOpCode(frameFormat &frame) {return frame & 15;}
    static inline unsigned char payloadLength(frameFormat &frame) {return (frame >> 8) & 127;}
    static inline bool rsv23(frameFormat &frame) {return frame & 48;}
    static inline bool rsv1(frameFormat &frame) {return frame & 64;}
    static inline bool getMask(frameFormat &frame) {return frame & 32768;}

    static inline void unmaskImprecise(char *dst, char *src, char *mask, unsigned int length)
    {
        for (unsigned int n = (length >> 2) + 1; n; n--) {
            *(dst++) = *(src++) ^ mask[0];
            *(dst++) = *(src++) ^ mask[1];
            *(dst++) = *(src++) ^ mask[2];
            *(dst++) = *(src++) ^ mask[3];
        }
    }

    static inline void unmaskImpreciseCopyMask(char *dst, char *src, char *maskPtr, unsigned int length)
    {
        char mask[4] = {maskPtr[0], maskPtr[1], maskPtr[2], maskPtr[3]};
        unmaskImprecise(dst, src, mask, length);
    }

    static inline void rotateMask(unsigned int offset, char *mask)
    {
        char originalMask[4] = {mask[0], mask[1], mask[2], mask[3]};
        mask[(0 + offset) % 4] = originalMask[0];
        mask[(1 + offset) % 4] = originalMask[1];
        mask[(2 + offset) % 4] = originalMask[2];
        mask[(3 + offset) % 4] = originalMask[3];
    }

    static inline void unmaskInplace(char *data, char *stop, char *mask)
    {
        while (data < stop) {
            *(data++) ^= mask[0];
            *(data++) ^= mask[1];
            *(data++) ^= mask[2];
            *(data++) ^= mask[3];
        }
    }

    enum state_t {
        READ_HEAD,
        READ_MESSAGE
    };

    enum send_state_t {
        SND_CONTINUATION = 1,
        SND_NO_FIN = 2,
        SND_COMPRESSED = 64
    };

    template <const int MESSAGE_HEADER, typename T>
    inline bool consumeMessage(T payLength, char *&src, unsigned int &length, frameFormat frame, void *user) {
        if (getOpCode(frame)) {
            if (opStack == 1 || (!lastFin && getOpCode(frame) < 2)) {
                forceClose(user);
                return true;
            }
            opCode[(unsigned char) ++opStack] = (OpCode) getOpCode(frame);
        } else if (opStack == -1) {
            forceClose(user);
            return true;
        }
        lastFin = isFin(frame);

        if (refusePayloadLength(user, payLength)) {
            forceClose(user);
            return true;
        }

        if (int(payLength) <= int(length - MESSAGE_HEADER)) {
            if (isServer) {
                unmaskImpreciseCopyMask(src, src + MESSAGE_HEADER, src + MESSAGE_HEADER - 4, payLength);
                if (handleFragment(src, payLength, 0, opCode[(unsigned char) opStack], isFin(frame), user)) {
                    return true;
                }
            } else {
                if (handleFragment(src + MESSAGE_HEADER, payLength, 0, opCode[(unsigned char) opStack], isFin(frame), user)) {
                    return true;
                }
            }

            if (isFin(frame)) {
                opStack--;
            }

            src += payLength + MESSAGE_HEADER;
            length -= payLength + MESSAGE_HEADER;
            spillLength = 0;
            return false;
        } else {
            spillLength = 0;
            state = READ_MESSAGE;
            remainingBytes = payLength - length + MESSAGE_HEADER;

            if (isServer) {
                memcpy(mask, src + MESSAGE_HEADER - 4, 4);
                unmaskImprecise(src, src + MESSAGE_HEADER, mask, length);
                rotateMask(4 - (length - MESSAGE_HEADER) % 4, mask);
            } else {
                src += MESSAGE_HEADER;
            }
            handleFragment(src, length - MESSAGE_HEADER, remainingBytes, opCode[(unsigned char) opStack], isFin(frame), user);
            return true;
        }
    }

    inline bool consumeContinuation(char *&src, unsigned int &length, void *user) {
        if (remainingBytes <= length) {
            if (isServer) {
                int n = remainingBytes >> 2;
                unmaskInplace(src, src + n * 4, mask);
                for (int i = 0, s = remainingBytes % 4; i < s; i++) {
                    src[n * 4 + i] ^= mask[i];
                }
            }

            if (handleFragment(src, remainingBytes, 0, opCode[(unsigned char) opStack], lastFin, user)) {
                return false;
            }

            if (lastFin) {
                opStack--;
            }

            src += remainingBytes;
            length -= remainingBytes;
            state = READ_HEAD;
            return true;
        } else {
            if (isServer) {
                unmaskInplace(src, src + ((length >> 2) + 1) * 4, mask);
            }

            remainingBytes -= length;
            if (handleFragment(src, length, remainingBytes, opCode[(unsigned char) opStack], lastFin, user)) {
                return false;
            }

            if (isServer && length % 4) {
                rotateMask(4 - (length % 4), mask);
            }
            return false;
        }
    }

    // this can hold two states (1 bit)
    // this can hold length of spill (up to 16 = 4 bit)
    unsigned char state = READ_HEAD;
    unsigned char spillLength = 0; // remove this!
    char opStack = -1; // remove this too
    char lastFin = true; // hold in state!
    unsigned char spill[LONG_MESSAGE_HEADER - 1];
    unsigned int remainingBytes = 0; // denna kan hålla spillLength om state är READ_HEAD, och remainingBytes när state är annat?
    char mask[isServer ? 4 : 1];
    OpCode opCode[2];

public:
    WebSocketProtocol() {

    }

    // Based on utf8_check.c by Markus Kuhn, 2005
    // https://www.cl.cam.ac.uk/~mgk25/ucs/utf8_check.c
    // Optimized for predominantly 7-bit content, 2016
    static bool isValidUtf8(unsigned char *s, size_t length)
    {
        for (unsigned char *e = s + length; s != e; ) {
            if (s + 4 <= e && ((*(uint32_t *) s) & 0x80808080) == 0) {
                s += 4;
            } else {
                while (!(*s & 0x80)) {
                    if (++s == e) {
                        return true;
                    }
                }

                if ((s[0] & 0x60) == 0x40) {
                    if (s + 1 >= e || (s[1] & 0xc0) != 0x80 || (s[0] & 0xfe) == 0xc0) {
                        return false;
                    }
                    s += 2;
                } else if ((s[0] & 0xf0) == 0xe0) {
                    if (s + 2 >= e || (s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80 ||
                            (s[0] == 0xe0 && (s[1] & 0xe0) == 0x80) || (s[0] == 0xed && (s[1] & 0xe0) == 0xa0)) {
                        return false;
                    }
                    s += 3;
                } else if ((s[0] & 0xf8) == 0xf0) {
                    if (s + 3 >= e || (s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80 || (s[3] & 0xc0) != 0x80 ||
                            (s[0] == 0xf0 && (s[1] & 0xf0) == 0x80) || (s[0] == 0xf4 && s[1] > 0x8f) || s[0] > 0xf4) {
                        return false;
                    }
                    s += 4;
                } else {
                    return false;
                }
            }
        }
        return true;
    }

    struct CloseFrame {
        uint16_t code;
        char *message;
        size_t length;
    };

    static inline CloseFrame parseClosePayload(char *src, size_t length) {
        CloseFrame cf = {};
        if (length >= 2) {
            memcpy(&cf.code, src, 2);
            cf = {ntohs(cf.code), src + 2, length - 2};
            if (cf.code < 1000 || cf.code > 4999 || (cf.code > 1011 && cf.code < 4000) ||
                (cf.code >= 1004 && cf.code <= 1006) || !isValidUtf8((unsigned char *) cf.message, cf.length)) {
                return {};
            }
        }
        return cf;
    }

    static inline size_t formatClosePayload(char *dst, uint16_t code, const char *message, size_t length) {
        if (code) {
            code = htons(code);
            memcpy(dst, &code, 2);
            memcpy(dst + 2, message, length);
            return length + 2;
        }
        return 0;
    }

    static inline size_t formatMessage(char *dst, const char *src, size_t length, OpCode opCode, size_t reportedLength, bool compressed) {
        size_t messageLength;
        size_t headerLength;
        if (reportedLength < 126) {
            headerLength = 2;
            dst[1] = reportedLength;
        } else if (reportedLength <= UINT16_MAX) {
            headerLength = 4;
            dst[1] = 126;
            *((uint16_t *) &dst[2]) = htons(reportedLength);
        } else {
            headerLength = 10;
            dst[1] = 127;
            *((uint64_t *) &dst[2]) = htobe64(reportedLength);
        }

        int flags = 0;
        dst[0] = (flags & SND_NO_FIN ? 0 : 128) | (compressed ? SND_COMPRESSED : 0);
        if (!(flags & SND_CONTINUATION)) {
            dst[0] |= opCode;
        }

        char mask[4];
        if (!isServer) {
            dst[1] |= 0x80;
            uint32_t random = rand();
            memcpy(mask, &random, 4);
            memcpy(dst + headerLength, &random, 4);
            headerLength += 4;
        }

        messageLength = headerLength + length;
        memcpy(dst + headerLength, src, length);

        if (!isServer) {

            // overwrites up to 3 bytes outside of the given buffer!
            //WebSocketProtocol<isServer>::unmaskInplace(dst + headerLength, dst + headerLength + length, mask);

            // this is not optimal
            char *start = dst + headerLength;
            char *stop = start + length;
            int i = 0;
            while (start != stop) {
                (*start++) ^= mask[i++ % 4];
            }
        }
        return messageLength;
    }

    void consume(char *src, unsigned int length, void *user) {
        if (spillLength) {
            src -= spillLength;
            length += spillLength;
            memcpy(src, spill, spillLength);
        }
        if (state == READ_HEAD) {
            parseNext:
            for (frameFormat frame; length >= SHORT_MESSAGE_HEADER; ) {
                memcpy(&frame, src, sizeof(frameFormat));

                // invalid reserved bits / invalid opcodes / invalid control frames / set compressed frame
                if ((rsv1(frame) && !setCompressed(user)) || rsv23(frame) || (getOpCode(frame) > 2 && getOpCode(frame) < 8) ||
                    getOpCode(frame) > 10 || (getOpCode(frame) > 2 && (!isFin(frame) || payloadLength(frame) > 125))) {
                    forceClose(user);
                    return;
                }

                if (payloadLength(frame) < 126) {
                    if (consumeMessage<SHORT_MESSAGE_HEADER, uint8_t>(payloadLength(frame), src, length, frame, user)) {
                        return;
                    }
                } else if (payloadLength(frame) == 126) {
                    if (length < MEDIUM_MESSAGE_HEADER) {
                        break;
                    } else if(consumeMessage<MEDIUM_MESSAGE_HEADER, uint16_t>(ntohs(*(uint16_t *) &src[2]), src, length, frame, user)) {
                        return;
                    }
                } else if (length < LONG_MESSAGE_HEADER) {
                    break;
                } else if (consumeMessage<LONG_MESSAGE_HEADER, uint64_t>(be64toh(*(uint64_t *) &src[2]), src, length, frame, user)) {
                    return;
                }
            }
            if (length) {
                memcpy(spill, src, length);
                spillLength = length;
            }
        } else if (consumeContinuation(src, length, user)) {
            goto parseNext;
        }
    }

    static const int CONSUME_POST_PADDING = 18;
    static const int CONSUME_PRE_PADDING = LONG_MESSAGE_HEADER - 1;

    // events to be implemented by application (can't be inline currently)
    bool refusePayloadLength(void *user, int length);
    bool setCompressed(void *user);
    void forceClose(void *user);
    bool handleFragment(char *data, size_t length, unsigned int remainingBytes, int opCode, bool fin, void *user);
};

}

#endif // WEBSOCKETPROTOCOL_UWS_H
