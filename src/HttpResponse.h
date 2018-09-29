#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H

/* An HttpResponse is the channel on which you send back a response */

#include "AsyncSocket.h"
#include "HttpResponseData.h"
#include "Utilities.h"

namespace uWS {

/* Some pre-defined status constants to use with writeStatus */
const char *HTTP_200_OK = "200 OK";

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

public:

    using Super::close;

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

    /* End the response with an optional data chunk */
    void end(std::string_view data = {}) {
        writeStatus(HTTP_200_OK);

        HttpResponseData<SSL> *httpResponseData = getHttpResponseData();

        if (httpResponseData->state & HttpResponseData<SSL>::HTTP_WRITE_CALLED) {
            /* Do not allow sending 0 chunk here */
            if (data.length()) {
                Super::write("\r\n", 2);
                writeUnsignedHex(data.length());
                Super::write("\r\n", 2);
                Super::write(data.data(), data.length());
            }

            /* Terminating 0 chunk */
            Super::write("\r\n0\r\n\r\n", 7);
        } else {
            /* We have a known send size */
            Super::write("Content-Length: ", 16);
            writeUnsigned(data.length());
            Super::write("\r\n\r\n", 4);

            Super::write(data.data(), data.length());
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

        Super::write("\r\n", 2);
        writeUnsignedHex(data.length());
        Super::write("\r\n", 2);
        Super::write(data.data(), data.length());

        // are we corked still?
        return true;
    }

    // todo: share this code in a function
    bool tryEnd(std::string_view data, int totalSize = 0) {
        /* Write status if not already done */
        writeStatus(HTTP_200_OK);

        /* If no total size given then assume this chunk is everything */
        if (!totalSize) {
            totalSize = data.length();
        }

        HttpResponseData<SSL> *httpResponseData = getHttpResponseData();

        if (httpResponseData->state & HttpResponseData<SSL>::HTTP_WRITE_CALLED) {
            /* Do not allow sending 0 chunk here */
            if (data.length()) {
                Super::write("\r\n", 2);
                writeUnsignedHex(data.length());
                Super::write("\r\n", 2);

                // should be optional
                Super::write(data.data(), data.length());
            }

            /* Terminating 0 chunk */
            Super::write("\r\n0\r\n\r\n", 7);
        } else {
            // if not already ended! should we call tryWrite after this? we need an extra flag! end called!

            if (!(httpResponseData->state & HttpResponseData<SSL>::HTTP_END_CALLED)) {
                /* We have a known send size */
                Super::write("Content-Length: ", 16);
                writeUnsigned(/*data.length()*/totalSize);
                Super::write("\r\n\r\n", 4);

                /* Mark end called */
                httpResponseData->state |= HttpResponseData<SSL>::HTTP_END_CALLED;
            }

            /* Write as much as possible without causing backpressure */
            int written = Super::write(data.data(), data.length(), true);

            httpResponseData->offset += written;
            //std::cout << "Offset is now: " << httpResponseData->offset << std::endl;

            return written == data.length();
        }

        // this path is completely wrong!
        //std::cout << "tryEnd returning " << (httpResponseData->offset == /*totalSize*/ data.length()) << std::endl;
        return httpResponseData->offset == /*totalSize*/ data.length();
    }

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
