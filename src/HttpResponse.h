/*
 * Copyright 2018 Alex Hultman and contributors.

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

#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H

/* An HttpResponse is the channel on which you send back a response */

#include "AsyncSocket.h"
#include "HttpResponseData.h"
#include "HttpContextData.h"
#include "Utilities.h"

/* todo: tryWrite is missing currently, only send smaller segments with write */

namespace uWS {

/* Some pre-defined status constants to use with writeStatus */
static const char *HTTP_200_OK = "200 OK";

/* The general timeout for HTTP sockets */
static const int HTTP_TIMEOUT_S = 10;

template <bool SSL>
struct HttpResponse : public AsyncSocket<SSL> {
    typedef AsyncSocket<SSL> Super;
private:
    HttpResponseData<SSL> *getHttpResponseData() {
        return (HttpResponseData<SSL> *) Super::getExt();
    }

    /* Write an unsigned 32-bit integer in hex */
    void writeUnsignedHex(unsigned int value) {
        char buf[10];
        int length = utils::u32toaHex(value, buf);

        /* For now we do this copy */
        Super::write(buf, length);
    }

    /* Write an unsigned 32-bit integer */
    void writeUnsigned(unsigned int value) {
        char buf[10];
        int length = utils::u32toa(value, buf);

        /* For now we do this copy */
        Super::write(buf, length);
    }

    /* Returns true on success, indicating that it might be feasible to write more data.
     * Will start timeout if stream reaches totalSize or write failure. */
    bool internalEnd(std::string_view data, int totalSize, bool optional) {
        /* Write status if not already done */
        writeStatus(HTTP_200_OK);

        /* If no total size given then assume this chunk is everything */
        if (!totalSize) {
            totalSize = data.length();
        }

        HttpResponseData<SSL> *httpResponseData = getHttpResponseData();
        if (httpResponseData->state & HttpResponseData<SSL>::HTTP_WRITE_CALLED) {

            /* We do not have tryWrite-like functionalities, so ignore optional in this path */

            /* Do not allow sending 0 chunk here */
            if (data.length()) {
                Super::write("\r\n", 2);
                writeUnsignedHex(data.length());
                Super::write("\r\n", 2);

                /* Ignoring optional for now */
                Super::write(data.data(), data.length());
            }

            /* Terminating 0 chunk */
            Super::write("\r\n0\r\n\r\n", 7);

            /* tryEnd can never fail when in chunked mode, since we do not have tryWrite (yet), only write */
            Super::timeout(HTTP_TIMEOUT_S);
            return true;
        } else {
            /* Write content-length on first call */
            if (!(httpResponseData->state & HttpResponseData<SSL>::HTTP_END_CALLED)) {
                /* We have a known send size */
                Super::write("Content-Length: ", 16);
                writeUnsigned(totalSize);
                Super::write("\r\n\r\n", 4);

                /* Mark end called */
                httpResponseData->state |= HttpResponseData<SSL>::HTTP_END_CALLED;
            }

            /* Even if we supply no new data to write, its failed boolean is useful to know
             * if it failed to drain any prior failed header writes */

            /* Write as much as possible without causing backpressure */
            auto [written, failed] = Super::write(data.data(), data.length(), optional);
            httpResponseData->offset += written;

            /* Success is when we wrote the entire thing without any failures */
            bool success = written == data.length() && !failed;

            /* If we are now at the end, start a timeout. Also start a timeout if we failed. */
            if (!success || httpResponseData->offset == totalSize) {
                Super::timeout(HTTP_TIMEOUT_S);
            }

            /* Remove onAborted function if we reach the end */
            if (httpResponseData->offset == totalSize) {
                httpResponseData->onAborted = nullptr;
                /* Also remove onWritable so that we do not emit when draining behind the scenes. */
                httpResponseData->onWritable = nullptr;
            }

            return success;
        }
    }

public:

    // this should probably not be public
    bool upgradeToWebSocket(void *newSocket) {

        HttpResponseData<SSL> *httpResponseData = getHttpResponseData();


        // this flag is not really needed to keep state of, could be per http content state
        httpResponseData->state |= HttpResponseData<SSL>::HTTP_UPGRADED_TO_WEBSOCKET;

        // also set pointer to the websocketcontext?

        // you can just check if the context of the socket changed? buy youi don't know the socket ptr!

        HttpContextData<SSL> *httpContextData = (HttpContextData<SSL> *) Super::static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)(

                    Super::static_dispatch(us_ssl_socket_get_context, us_socket_get_context)((typename Super::SOCKET_TYPE *) this)

                    );

        httpContextData->upgradedWebSocket = newSocket;

        // I jhave no idea what this does
        return true;


        /*SOCKET_CONTEXT_TYPE *getSocketContext() {
            return (SOCKET_CONTEXT_TYPE *) this;
        }

        static SOCKET_CONTEXT_TYPE *getSocketContext(SOCKET_TYPE *s) {
            return (SOCKET_CONTEXT_TYPE *) static_dispatch(us_ssl_socket_get_context, us_socket_get_context)(s);
        }

        HttpContextData<SSL> *getSocketContextData() {
            return (HttpContextData<SSL> *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)(getSocketContext());
        }

        static HttpContextData<SSL> *getSocketContextDataS(SOCKET_TYPE *s) {
            return (HttpContextData<SSL> *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)(getSocketContext(s));
        }*/

        //return (HttpContextData<SSL> *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)(getSocketContext(s));
    }

    /* Immediately terminate this Http response */
    using Super::close;

    /* Note: Headers are not checked in regards to timeout.
     * We only check when you actively push data or end the request */

    /* Write the HTTP status */
    HttpResponse *writeStatus(std::string_view status) {
        HttpResponseData<SSL> *httpResponseData = getHttpResponseData();

        /* Do not allow writing more than one status */
        if (httpResponseData->state & HttpResponseData<SSL>::HTTP_STATUS_CALLED) {
            return this;
        }

        /* Update status */
        httpResponseData->state |= HttpResponseData<SSL>::HTTP_STATUS_CALLED;

        Super::write("HTTP/1.1 ", 9);
        Super::write(status.data(), status.length());
        Super::write("\r\n", 2);
        return this;
    }

    /* Write an HTTP header with string value */
    HttpResponse *writeHeader(std::string_view key, std::string_view value) {
        writeStatus(HTTP_200_OK);

        Super::write(key.data(), key.length());
        Super::write(": ", 2);
        Super::write(value.data(), value.length());
        Super::write("\r\n", 2);
        return this;
    }

    /* Write an HTTP header with unsigned int value */
    HttpResponse *writeHeader(std::string_view key, unsigned int value) {
        Super::write(key.data(), key.length());
        Super::write(": ", 2);
        writeUnsigned(value);
        Super::write("\r\n", 2);
        return this;
    }

    /* End the response with an optional data chunk. Always starts a timeout. */
    void end(std::string_view data = {}) {
        internalEnd(data, data.length(), false);
    }

    /* Try and end the response. Returns true on success. Starts a timeout in some cases. */
    bool tryEnd(std::string_view data, int totalSize = 0) {
        return internalEnd(data, totalSize, true);
    }

    /* Write parts of the response in chunking fashion. Starts timeout if failed. */
    bool write(std::string_view data) {
        writeStatus(HTTP_200_OK);

        /* Do not allow sending 0 chunks, they mark end of response */
        if (!data.length()) {
            /* If you called us, then according to you it was fine to call us so it's fine to still call us */
            return true;
        }

        HttpResponseData<SSL> *httpResponseData = getHttpResponseData();

        if (!(httpResponseData->state & HttpResponseData<SSL>::HTTP_WRITE_CALLED)) {
            writeHeader("Transfer-Encoding", "chunked");
            httpResponseData->state |= HttpResponseData<SSL>::HTTP_WRITE_CALLED;
        }

        Super::write("\r\n", 2);
        writeUnsignedHex(data.length());
        Super::write("\r\n", 2);

        auto [written, failed] = Super::write(data.data(), data.length());
        if (failed) {
            Super::timeout(HTTP_TIMEOUT_S);
        }

        /* If we did not fail the write, accept more */
        return !failed;
    }

    /* Get the current byte write offset for this Http response */
    int getWriteOffset() {
        HttpResponseData<SSL> *httpResponseData = getHttpResponseData();

        return httpResponseData->offset;
    }

    /* Attach handler for writable HTTP response */
    HttpResponse *onWritable(std::function<bool(int)> handler) {
        HttpResponseData<SSL> *httpResponseData = getHttpResponseData();

        httpResponseData->onWritable = handler;
        return this;
    }

    /* Attach handler for aborted HTTP request */
    HttpResponse *onAborted(std::function<void()> handler) {
        HttpResponseData<SSL> *httpResponseData = getHttpResponseData();

        httpResponseData->onAborted = handler;
        return this;
    }

    // onData(chunk, remaining == -1 or 0 or actual remaining)?
    /* Attach a read handler for data sent. Will be called with a chunk of size 0 when FIN */
    void read(std::function<void(std::string_view)> handler) {
        HttpResponseData<SSL> *data = getHttpResponseData();
        data->inStream = handler;
    }
};

}

#endif // HTTPRESPONSE_H
