#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H

/* An HttpResponse is the channel on which you send back a response */

#include "AsyncSocket.h"
#include "HttpResponseData.h"

namespace uWS {

/* Some pre-defined status constants to use with writeStatus */
const char *HTTP_200_OK = "200 OK";

template <bool SSL>
struct HttpResponse : public AsyncSocket<SSL> {
private:
    HttpResponseData<SSL> *getHttpResponseData() {
        return (HttpResponseData<SSL> *) AsyncSocket<SSL>::getExt();
    }

    int u32toaHex(uint32_t value, char *dst) {
        char palette[] = "0123456789abcdef";
        char temp[10];
        char *p = temp;
        do {
            *p++ = palette[value % 16];
            value /= 16;
        } while (value > 0);

        int ret = p - temp;

        do {
            *dst++ = *--p;
        } while (p != temp);

        return ret;
    }

    /* Write an unsigned 32-bit integer in hex */
    void writeUnsignedHex(unsigned int value) {
        char buf[10];
        int length = u32toaHex(value, buf);

        /* For now we do this copy */
        AsyncSocket<SSL>::write(buf, length);
    }

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

    /* Write an unsigned 32-bit integer */
    void writeUnsigned(unsigned int value) {
        char buf[10];
        int length = u32toa(value, buf);

        /* For now we do this copy */
        AsyncSocket<SSL>::write(buf, length);
    }

public:
    /* Write the HTTP status */
    HttpResponse *writeStatus(std::string_view status) {
        HttpResponseData<SSL> *httpResponseData = getHttpResponseData();

        /* Do not allow writing more than one status */
        if (httpResponseData->state & HttpResponseData<SSL>::HTTP_STATUS_SENT) {
            return this;
        }

        /* Update status */
        httpResponseData->state |= HttpResponseData<SSL>::HTTP_STATUS_SENT;

        AsyncSocket<SSL>::write("HTTP/1.1 ", 9);
        AsyncSocket<SSL>::write(status.data(), status.length());
        AsyncSocket<SSL>::write("\r\n", 2);
        return this;
    }

    /* Write an HTTP header with string value */
    HttpResponse *writeHeader(std::string_view key, std::string_view value) {
        AsyncSocket<SSL>::write(key.data(), key.length());
        AsyncSocket<SSL>::write(": ", 2);
        AsyncSocket<SSL>::write(value.data(), value.length());
        AsyncSocket<SSL>::write("\r\n", 2);
        return this;
    }

    /* Write an HTTP header with unsigned int value */
    HttpResponse *writeHeader(std::string_view key, unsigned int value) {
        AsyncSocket<SSL>::write(key.data(), key.length());
        AsyncSocket<SSL>::write(": ", 2);
        writeUnsigned(value);
        AsyncSocket<SSL>::write("\r\n", 2);
        return this;
    }

    /* End the response with an optional data chunk */
    void end(std::string_view data = {}) {
        HttpResponseData<SSL> *httpResponseData = getHttpResponseData();

        if (httpResponseData->state & HttpResponseData<SSL>::HTTP_WRITE_CALLED) {
            /* Do not allow sending 0 chunk here */
            if (data.length()) {
                AsyncSocket<SSL>::write("\r\n", 2);
                writeUnsignedHex(data.length());
                AsyncSocket<SSL>::write("\r\n", 2);
                AsyncSocket<SSL>::write(data.data(), data.length());
            }

            /* Terminating 0 chunk */
            AsyncSocket<SSL>::write("\r\n0\r\n\r\n", 7);
        } else {
            /* We have a known send size */
            AsyncSocket<SSL>::write("Content-Length: ", 16);
            writeUnsigned(data.length());
            AsyncSocket<SSL>::write("\r\n\r\n", 4);

            AsyncSocket<SSL>::write(data.data(), data.length());
        }
    }

    /* Write parts of the response in chunking fashion */
    bool write(std::string_view data) {
        /* Do not allow sending 0 chunks, they mark end of response */
        if (!data.length()) {
            return true; // are we corked still?
        }

        HttpResponseData<SSL> *httpResponseData = getHttpResponseData();

        if (!(httpResponseData->state & HttpResponseData<SSL>::HTTP_WRITE_CALLED)) {
            writeHeader("Transfer-Encoding", "chunked");
            httpResponseData->state |= HttpResponseData<SSL>::HTTP_WRITE_CALLED;
        }

        AsyncSocket<SSL>::write("\r\n", 2);
        writeUnsignedHex(data.length());
        AsyncSocket<SSL>::write("\r\n", 2);
        AsyncSocket<SSL>::write(data.data(), data.length());

        // are we corked still?
        return true;
    }

    // we really want tryEnd(data) integer to try and stream something with known size

    // write/tryWrite called first should enter into chunked?

    // tryWrite(char *, length) int

    // write(char *, length) bool



    /* Attach an output stream function. Chunks may be read more than once. Negative offset mean broken stream */
    void writeOldRemoveMe(std::function<std::pair<bool, std::string_view>(int)> cb, int length = 0) {
        HttpResponseData<SSL> *httpResponseData = getHttpResponseData();

        /* Do not allow write if already called */
        if (httpResponseData->state & HttpResponseData<SSL>::HTTP_WRITE_CALLED) {
            return;
        }

        /* Write 200 OK if not already written any status */
        writeStatus(HTTP_200_OK);

        /* Update status */
        httpResponseData->state |= HttpResponseData<SSL>::HTTP_WRITE_CALLED;
        if (length) {
            httpResponseData->state |= HttpResponseData<SSL>::HTTP_KNOWN_STREAM_OUT_SIZE;

            /* Rely on FIN to signal end if we do not pass any length */
            AsyncSocket<SSL>::write("Content-Length: ", 16);
            writeUnsigned(length);
            AsyncSocket<SSL>::write("\r\n", 2);
        }

        /* HTTP body separator */
        AsyncSocket<SSL>::write("\r\n", 2);

        for (int offset = 0; length == 0 || offset < length; ) {
            /* Pull a chunk from stream */
            auto [msg_more, chunk] = cb(offset);

            /* Handle PAUSE and FIN */
            if (chunk.length() == 0) {
                /* FIN */
                if (chunk.data()) {
                    /* Try flush and shut down */
                    AsyncSocket<SSL>::uncork();
                    if (!AsyncSocket<SSL>::hasBuffer()) {
                        /* Uncork finished with no buffered data */
                        AsyncSocket<SSL>::shutdown();
                    } else {
                        /* Let it shut down when drained */
                        httpResponseData->state |= HttpResponseData<SSL>::HTTP_ENDED_STREAM_OUT;
                    }
                } else {
                    /* Disable timeout and mark this stream as paused (important to silence spurious onWritable events) */
                    AsyncSocket<SSL>::timeout(0);
                    httpResponseData->state |= HttpResponseData<SSL>::HTTP_PAUSED_STREAM_OUT;

                    // forgot about this path, if we sent things off and then ended up pausing!
                    httpResponseData->offset = offset;
                    httpResponseData->outStream = cb;
                }
                return;
            }

            /* Send off the chunk */
            int written = AsyncSocket<SSL>::write(chunk.data(), chunk.length(), true);

            /* If we failed to send everything, exit */
            if (written < chunk.length()) {
                httpResponseData->offset = offset + written;
                httpResponseData->outStream = cb;
                return;
            }

            offset += written;
        }
    }

    /* Attach a read handler for data sent. Will be called with a chunk of size 0 when FIN */
    void read(std::function<void(std::string_view)> handler) {
        HttpResponseData<SSL> *data = getHttpResponseData();
        data->inStream = handler;
    }
};

}

#endif // HTTPRESPONSE_H
