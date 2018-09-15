#ifndef ASYNCSOCKET_H
#define ASYNCSOCKET_H

/* This class implements async socket memory management strategies */

#include "StaticDispatch.h"
#include "LoopData.h"
#include "AsyncSocketData.h"

namespace uWS {

template <bool SSL>
struct AsyncSocket : StaticDispatch<SSL> {
protected:
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
        if constexpr(SSL) {
            return (LoopData *) us_loop_ext(us_ssl_socket_context_loop(us_ssl_socket_get_context((SOCKET_TYPE *) this)));
        } else {
            return (LoopData *) us_loop_ext(us_socket_context_loop(us_socket_get_context((SOCKET_TYPE *) this)));
        }
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

        std::cout << "Write called with length: " << length << ", optionally: " << optionally << std::endl;

        /* Do nothing for a null sized chunk */
        if (length == 0) {
            std::cout << "Write returned: 0" << std::endl;
            return 0;
        }

        AsyncSocketData<SSL> *asyncSocketData = (AsyncSocketData<SSL> *) getExt();

        /* Do not write anything if we have a per-socket buffer */
        if (asyncSocketData->buffer.length()) {
            if (optionally) {
                std::cout << "Write returned: 0" << std::endl;
                return 0;
            } else {
                std::cout << "Buffering at top of write!" << std::endl;

                /* At least we can reserve room for next chunk if we know it up front */
                if (nextLength) {
                    asyncSocketData->buffer.reserve(asyncSocketData->buffer.length() + length + nextLength);
                }

                /* Buffer this chunk */
                asyncSocketData->buffer.append(src, length);
                std::cout << "Write returned: " << length << std::endl;
                return length;
            }
        }

        if (loopData->corked) {
            /* We are corked */
            if (LoopData::CORK_BUFFER_SIZE - loopData->corkOffset >= length) {
                /* If the entire chunk fits in cork buffer */
                memcpy(loopData->corkBuffer + loopData->corkOffset, src, length);
                loopData->corkOffset += length;
            } else {
                /* Strategy differences between SSL and non-SSL */
                if constexpr(SSL) {
                    /* Cork up as much as we can, optionally does not matter here as we know it will fit in cork */
                    int written = write(src, std::min(LoopData::CORK_BUFFER_SIZE - loopData->corkOffset, length), false, 0);

                    /* Optionally matters here though */
                    written += uncork(src + written, length - written, optionally);
                    std::cout << "Write returned: " << written << std::endl;
                    return written;
                } else {
                    /* For non-SSL we take the penalty of two syscalls */
                    int written = uncork(src, length, optionally);
                    std::cout << "Write returned: " << written << std::endl;
                    return written;
                }
            }
        } else {
            /* We are not corked */
            int written = static_dispatch(us_ssl_socket_write, us_socket_write)((SOCKET_TYPE *) this, src, length, nextLength != 0);

            /* Did we fail? */
            if (written < length) {
                /* If the write was optional then just bail out */
                if (optionally) {
                    std::cout << "Write returned: " << written << std::endl;
                    return written;
                }

                std::cout << "Buffering at bottom of write!" << std::endl;

                /* Fall back to worst possible case (should be very rare for HTTP) */
                /* At least we can reserve room for next chunk if we know it up front */
                if (nextLength) {
                    asyncSocketData->buffer.reserve(asyncSocketData->buffer.length() + length + nextLength);
                }

                /* Buffer this chunk */
                asyncSocketData->buffer.append(src, length);
            }
        }

        std::cout << "Write returned: " << length << std::endl;
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

    /* Uncork this socket and flush or buffer any corked and/or passed data. It is essential to remember doing this. */
    /* It does NOT count bytes written from cork buffer (they are already accounted for in the write call responsible for its corking)! */
    int uncork(const char *src = nullptr, int length = 0, bool optionally = false) {
        LoopData *loopData = getLoopData();

        if (loopData->corked) {
            loopData->corked = false;

            if (loopData->corkOffset) {
                /* Corked data is already accounted for via its write call */
                write(loopData->corkBuffer, loopData->corkOffset, false, length);
                loopData->corkOffset = 0;
            }

            /* We should only return with new writes, not things written to cork already */
            return write(src, length, optionally, 0);
        }
    }

    /* Drain any socket-buffer while also optionally sending a chunk */
    int mergeDrain(std::string_view optionalChunk) {

        // strategy: if we have two parts and both will fit in cork buffer then cork them and recursively send them off

        // write any per-socket buffer and optionally more

        AsyncSocketData<SSL> *asyncSocketData = (AsyncSocketData<SSL> *) getExt();

        // not handled yet
        if (asyncSocketData->buffer.length()) {
            std::cout << "ERROR! has socket buffer!" << std::endl;
            exit(0);
        }


        /* Write the optional part */
        return write(optionalChunk.data(), optionalChunk.length(), true, 0);
    }

    void close() {
        static_dispatch(us_ssl_socket_close, us_socket_close)((SOCKET_TYPE *) this);
    }
};

}

#endif // ASYNCSOCKET_H
