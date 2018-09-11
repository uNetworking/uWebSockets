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

    // cork should bascially mark corked
    // if already corked, crash and burn!
    void cork() {
        LoopData *loopData = getLoopData();
        loopData->corked = true;
    }

    // if not in corked mode, write & buffer. if in corked mode: either write & buffer or buffer
    // that is, corking is optional
    void write(const char *src, int length) {
        LoopData *loopData = getLoopData();

        // append it
        memcpy(loopData->corkBuffer + loopData->corkOffset, src, length);
        loopData->corkOffset += length;
    }

    // should follow same rules as for write
    void writeUnsigned(unsigned int value) {
        LoopData *loopData = getLoopData();

        loopData->corkOffset += u32toa(value, loopData->corkBuffer + loopData->corkOffset);
    }

    // uncork should always send and clean the loop's cork buffer. anything that is not able to send, buffer it up in the socket
    void uncork() {
        LoopData *loopData = getLoopData();

        loopData->corked = false;

        // send it off now!

        int written = static_dispatch(us_ssl_socket_write, us_socket_write)((SOCKET_TYPE *) this, loopData->corkBuffer, loopData->corkOffset, false);

        loopData->corkOffset = 0;

        // buffer the rest up in the outbuffer of this asynsocket!
    }

    // when we are writable AND have buffer length, that's a really bad situation that should not happen!
    void drain(std::string_view optionalChunk) {

        // the question here is: should we recursively copy things to the cork buffer and try and send them off in one go?
        // it would work in a recursively manner and since optionalChunk is optional, it would never increase the buffer size

        // this function is called from onWritable and combines any buffered data with the chunk

        // the cunk is optional meaning we do not care to buffer it up
    }

    // this one will write up to length and simply leave things be
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

    void close() {
        static_dispatch(us_ssl_socket_close, us_socket_close)((SOCKET_TYPE *) this);
    }
};

}

#endif // ASYNCSOCKET_H
