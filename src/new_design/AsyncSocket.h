#ifndef ASYNCSOCKET_H
#define ASYNCSOCKET_H

#include "StaticDispatch.h"
#include "LoopData.h"

// todo: this is where the magic happens

namespace uWS {

template <bool SSL>
struct AsyncSocket : StaticDispatch<SSL> {

    using SOCKET_TYPE = typename StaticDispatch<SSL>::SOCKET_TYPE;
    using StaticDispatch<SSL>::static_dispatch;

    // control everything with write, cork, uncork, close
    // have HttpResponseData derive from AsyncSocketData?

    // HttpResponse will only need one buffer for outgoing - the corked up

    // maybe better name would be CorkableSocket?

    // this does not belong here!
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

    void cork() {
        LoopData *loopData = (LoopData *) us_loop_ext(us_socket_context_loop(us_socket_get_context((us_socket *) this)));

        loopData->corked = true;

    }

    void uncork() {
        LoopData *loopData = (LoopData *) us_loop_ext(us_socket_context_loop(us_socket_get_context((us_socket *) this)));

        loopData->corked = false;

        // send it off now!

        int written = static_dispatch(us_ssl_socket_write, us_socket_write)((SOCKET_TYPE *) this, loopData->corkBuffer, loopData->corkOffset, false);

        loopData->corkOffset = 0;

        // buffer the rest up in the outbuffer of this asynsocket!
    }

    void writeUnsigned(unsigned int value) {
        LoopData *loopData = (LoopData *) us_loop_ext(us_socket_context_loop(us_socket_get_context((us_socket *) this)));

        loopData->corkOffset += u32toa(value, loopData->corkBuffer + loopData->corkOffset);
    }

    void write(const char *src, int length) {
        LoopData *loopData = (LoopData *) us_loop_ext(us_socket_context_loop(us_socket_get_context((us_socket *) this)));

        memcpy(loopData->corkBuffer + loopData->corkOffset, src, length);
        loopData->corkOffset += length;
    }

    int writeOptionally(const char *src, int length) {

        // not optional for now
        AsyncSocket<SSL>::write(src, length);

        return length;

/*
        // kopiera upp till (SSL eller icke-ssl) max copy distance

        // om mer än detta, fortsätt skicka

        // this entire strategy should be made entirely in AsyncSocket!

        // this strategy can be simplified to one, we can even have MAX_COPY_DISTANCE_SSL and MAX_COPY_DISTANCE
        if (length < LoopData::MAX_COPY_DISTANCE) {
            AsyncSocket<SSL>::write("Content-Length: ", 16);
            AsyncSocket<SSL>::writeUnsigned(chunk.length());
            AsyncSocket<SSL>::write("\r\n\r\n", 4);
            AsyncSocket<SSL>::write(chunk.data(), chunk.length());
        } else {
            // copying some data with the headers is a good idea for SSL but probably not for non-SSL
            //writeToCorkBufferAndReset(chunk.data(), LoopData::MAX_COPY_DISTANCE, length, true);

            // just assume this went fine
            HttpResponseData<SSL> *httpData = (HttpResponseData<SSL> *) static_dispatch(us_ssl_socket_ext, us_socket_ext)((SOCKET_TYPE *) this);

            // write that off! (should never happen here!)
            //static_dispatch(us_ssl_socket_write, us_socket_write)((SOCKET_TYPE *) this, chunk.data() + LoopData::MAX_COPY_DISTANCE, chunk.length() - LoopData::MAX_COPY_DISTANCE, 0);

            // if offset is at the end, we are done
            if (httpData->offset < length) {
                httpData->outStream = cb;
            }
        }*/

    }

    void *getExt() {
        return static_dispatch(us_ssl_socket_ext, us_socket_ext)((SOCKET_TYPE *) this);
    }

    void close() {
        static_dispatch(us_ssl_socket_close, us_socket_close)((SOCKET_TYPE *) this);
    }
};

}

#endif // ASYNCSOCKET_H
