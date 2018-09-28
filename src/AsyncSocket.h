#ifndef ASYNCSOCKET_H
#define ASYNCSOCKET_H

/* This class implements async socket memory management strategies */

#include "StaticDispatch.h"
#include "LoopData.h"
#include "AsyncSocketData.h"

namespace uWS {

template <bool SSL>
struct AsyncSocket : StaticDispatch<SSL> {
    template <bool> friend struct HttpContext;
protected:
    using SOCKET_TYPE = typename StaticDispatch<SSL>::SOCKET_TYPE;
    using StaticDispatch<SSL>::static_dispatch;

    LoopData *getLoopData() {
        if constexpr(SSL) {
            return (LoopData *) us_loop_ext(us_ssl_socket_context_loop(us_ssl_socket_get_context((SOCKET_TYPE *) this)));
        } else {
            return (LoopData *) us_loop_ext(us_socket_context_loop(us_socket_get_context((SOCKET_TYPE *) this)));
        }
    }

    void *getExt() {
        return static_dispatch(us_ssl_socket_ext, us_socket_ext)((SOCKET_TYPE *) this);
    }

    void timeout(unsigned int seconds) {
        static_dispatch(us_ssl_socket_timeout, us_socket_timeout)((SOCKET_TYPE *) this, seconds);
    }

    void shutdown() {
        static_dispatch(us_ssl_socket_shutdown, us_socket_shutdown)((SOCKET_TYPE *) this);
    }

    SOCKET_TYPE *close() {
        return static_dispatch(us_ssl_socket_close, us_socket_close)((SOCKET_TYPE *) this);
    }

    /* Cork this socket. Only one socket may ever be corked per-loop at any given time */
    void cork() {
        std::cout << "Cork called" << std::endl;

        LoopData *loopData = getLoopData();
        loopData->corked = true;
    }

    /* Write in three levels of prioritization: cork-buffer, syscall, socket-buffer. Always drain if possible. */
    // todo: consider supporting nextLength = UNKNOWN as -1 (more but unknown size)
    int write(const char *src, int length, bool optionally = false, int nextLength = 0) {
        LoopData *loopData = getLoopData();

        std::cout << "Write called with length: " << length << ", optionally: " << optionally << std::endl;

        AsyncSocketData<SSL> *asyncSocketData = (AsyncSocketData<SSL> *) getExt();

        /* Do nothing for a null sized chunk */
        if (length == 0 && !asyncSocketData->buffer.length()) {
            //std::cout << "Write returned: 0" << std::endl;
            return 0;
        }

        /* Do not write anything if we have a per-socket buffer */
        if (asyncSocketData->buffer.length()) {
            if (optionally) {


                // we have buffer and we are optionally, if drain then drain else quit

                // drain here
                std::cout << "Drain path" << std::endl;

                // will just end up in a loop!
                int written = static_dispatch(us_ssl_socket_write, us_socket_write)((SOCKET_TYPE *) this, asyncSocketData->buffer.data(), asyncSocketData->buffer.length(), nextLength != 0);//write(asyncSocketData->buffer.data(), asyncSocketData->buffer.length(), optionally, 0, true);

                // removeBuffer
                asyncSocketData->buffer = asyncSocketData->buffer.substr(written);

                // should we really return this here? should be 0 as we took 0 new data!
                return 0;



            } else {
                std::cout << "Buffering at top of write (really bad)!" << std::endl;

                /* At least we can reserve room for next chunk if we know it up front */
                if (nextLength) {
                    asyncSocketData->buffer.reserve(asyncSocketData->buffer.length() + length + nextLength);
                }

                /* Buffer this chunk */
                asyncSocketData->buffer.append(src, length);
                //std::cout << "Write returned: " << length << std::endl;
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
                    //std::cout << "Write returned: " << written << std::endl;
                    return written;
                } else {
                    /* For non-SSL we take the penalty of two syscalls */
                    int written = uncork(src, length, optionally);
                    //std::cout << "Write returned: " << written << std::endl;
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
                    //std::cout << "Write returned: " << written << std::endl;
                    return written;
                }

                std::cout << "Buffering at bottom of write (okay)!" << std::endl;

                /* Fall back to worst possible case (should be very rare for HTTP) */
                /* At least we can reserve room for next chunk if we know it up front */
                if (nextLength) {
                    asyncSocketData->buffer.reserve(asyncSocketData->buffer.length() + length - written + nextLength);
                }

                /* Buffer this chunk */
                asyncSocketData->buffer.append(src + written, length - written);
            }
        }

        //std::cout << "Write returned: " << length << std::endl;
        return length;
    }

    /* Uncork this socket and flush or buffer any corked and/or passed data. It is essential to remember doing this. */
    /* It does NOT count bytes written from cork buffer (they are already accounted for in the write call responsible for its corking)! */
    int uncork(const char *src = nullptr, int length = 0, bool optionally = false) {

        std::cout << "Uncork called with length: " << length << std::endl;

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
        } else {
            std::cout << "Not even corked!" << std::endl;
        }

        return 0;
    }
};

}

#endif // ASYNCSOCKET_H
