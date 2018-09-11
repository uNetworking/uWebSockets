#ifndef ASYNCSOCKET_H
#define ASYNCSOCKET_H

#include "StaticDispatch.h"
#include "LoopData.h"
#include "AsyncSocketData.h"

// todo: this is where the magic happens

namespace uWS {

template <bool SSL>
struct AsyncSocket : StaticDispatch<SSL> {
protected:
    // this will probably be different on ssl and non-ssl
    static const int MAX_COPY_DISTANCE = 4096;

    using SOCKET_TYPE = typename StaticDispatch<SSL>::SOCKET_TYPE;
    using StaticDispatch<SSL>::static_dispatch;

    // this will have to belong here for now
    int u32toa(uint32_t value, char *dst) {
        char temp[10];
        char *p = temp;
        do {
            *p++ = (char) (value % 10) + '0';
            value /= 10;
        } while (value > 0);

        int ret = p - temp;

        do {
            *dst++ = *--p;
        } while (p != temp);

        return ret;
    }

    LoopData *getLoopData() {
        return (LoopData *) us_loop_ext(us_socket_context_loop(us_socket_get_context((SOCKET_TYPE *) this)));
    }

public:
    void *getExt() {
        return static_dispatch(us_ssl_socket_ext, us_socket_ext)((SOCKET_TYPE *) this);
    }

    /* Cork this socket. Only one socket may ever be corked per-loop at any given time */
    void cork() {
        LoopData *loopData = getLoopData();
        loopData->corked = true;
    }

    /* Write in three levels of prioritization: cork-buffer, syscall, socket-buffer */
    int write(const char *src, int length, bool optionally = false, int nextLength = 0) {
        LoopData *loopData = getLoopData();

        if (length == 0) {
            return 0;
        }

        if (loopData->corked) {

            // prio 1: write to cork
            if (LoopData::CORK_BUFFER_SIZE - loopData->corkOffset >= length) {
                memcpy(loopData->corkBuffer + loopData->corkOffset, src, length);
                loopData->corkOffset += length;
            } else {
                /* Strategy differences between SSL and non-SSL */
                if constexpr(SSL) {
                    /* For SSL we do not want to emit small chunks, so fill the cork (assuming the cork is about 16k) */
                    int remainingCork = LoopData::CORK_BUFFER_SIZE - loopData->corkOffset;

                    memcpy(loopData->corkBuffer + loopData->corkOffset, src, remainingCork);
                    loopData->corkOffset += remainingCork;

                    uncork(src + remainingCork, length - remainingCork);
                } else {
                    /* For non-SSL we take the penalty of two syscalls */
                    uncork(src, length);
                }
            }
        } else {
            // not corked
            // we should not write here if if have buffer!
            int written = 0;

            AsyncSocketData<SSL> *asyncSocketData = (AsyncSocketData<SSL> *) getExt();

            if (!asyncSocketData->buffer.length()) {
                written = static_dispatch(us_ssl_socket_write, us_socket_write)((SOCKET_TYPE *) this, src, length, nextLength != 0);
            }


            if (written < length) {

                // bail out if we can
                if (optionally) {
                    return written;
                }

                // shit, we are fucked
                std::cout << "Buffering up per-socket" << std::endl;
                AsyncSocketData<SSL> *asyncSocketData = (AsyncSocketData<SSL> *) getExt();

                // at least reserve enough for next failure
                if (nextLength) {
                    asyncSocketData->buffer.reserve(asyncSocketData->buffer.length() + length + nextLength);
                }

                asyncSocketData->buffer.append(src, length);
            }
        }

        return length;
    }

    /* Write an unsigned 32-bit integer */
    void writeUnsigned(unsigned int value) {
        LoopData *loopData = getLoopData();

        char buf[10];
        int length = u32toa(value, buf);

        // for now we do this copy
        write(buf, length);
    }

    /* Uncork this socket. It is essential to remember doing this. */
    void uncork(const char *src = nullptr, int length = 0) {
        LoopData *loopData = getLoopData();

        if (loopData->corked) {
            loopData->corked = false;

            if (loopData->corkOffset) {
                write(loopData->corkBuffer, loopData->corkOffset, false, length);
                write(src, length, false, 0);
                loopData->corkOffset = 0;
            }
        }
    }

    /* Drain any socket-buffer while also optionally sending a chunk */
    void mergeDrain(std::string_view optionalChunk) {

        std::cout << "mergeDrain" << std::endl;

        // the question here is: should we recursively copy things to the cork buffer and try and send them off in one go?
        // it would work in a recursively manner and since optionalChunk is optional, it would never increase the buffer size

        // this function is called from onWritable and combines any buffered data with the chunk

        // the cunk is optional meaning we do not care to buffer it up
    }

    void close() {
        static_dispatch(us_ssl_socket_close, us_socket_close)((SOCKET_TYPE *) this);
    }
};

}

#endif // ASYNCSOCKET_H
