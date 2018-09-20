#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H

/* An HttpResponse is the channel on which you send back a response */

#include "AsyncSocket.h"
#include "HttpResponseData.h"

namespace uWS {

/* Some pre-defined status constants to use with writeStatus */
const char *HTTP_200_OK = "200 OK";

/* Return this from a stream callback to signal pause */
const std::pair<bool, std::string_view> HTTP_STREAM_PAUSE = {false, std::string_view(nullptr, 0)};

/* Return this from a stream callback to signal FIN */
const std::pair<bool, std::string_view> HTTP_STREAM_FIN = {false, std::string_view((const char *) 1, 0)};

template <bool SSL>
struct HttpResponse : public AsyncSocket<SSL> {
private:
    HttpResponseData<SSL> *getHttpResponseData() {
        return (HttpResponseData<SSL> *) AsyncSocket<SSL>::getExt();
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

    /* Resume response streaming as far as possible */
    void resume(std::string_view chunk = {}) {
        HttpResponseData<SSL> *httpResponseData = getHttpResponseData();

        /* Do nothing if not even paused */
        if (!(httpResponseData->state & HttpResponseData<SSL>::HTTP_PAUSED_STREAM_OUT)) {
            return;
        }

        /* Remove paused status */
        httpResponseData->state &= ~HttpResponseData<SSL>::HTTP_PAUSED_STREAM_OUT;

        int written = AsyncSocket<SSL>::write(chunk.data(), chunk.length(), true);

        if (written == chunk.length()) {
            // pull a new chunk from the callback (basically call onWritable)
        }

        // no, basically just write this off and if all written, call streamOut callback
    }

    /* Attach a write handler for sending data. Length must be specified up front and chunks might be read more than once */

    // chunk = stream(int offset) is current interface

    // we need more information such as:

    // 1. (RETURN) was the short response of data meant to be msg_more-d? basically: should the stream be called again? needed to allow expose of msg_more to streamer!

    // 2. (RETURN) what data are we returning, or are we returning no data (pause signal)

    // 3. (RETURN) how do we signal FIN for length-less data? return a different string_view with 0 length? HTTP_STREAM_FIN?

    // 4. (INPUT ARGS) how much has been acked of what we send? int offset is an indication? what about indicating ONLY the ack?


    void write(std::function<std::pair<bool, std::string_view>(int)> cb, int length = 0) {
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

        // int tail, int head. tail is what has been sent off, head is where we read from. they can differ - does it matter?

        // just do resume() and pause() to throttle onwritable calling
        // also add bool isAck to writable stream to be treated as ack signalling  (return is never mind)

        // we need resume() to resume sending (very simple to just call the onWritable callback again)
        // returning less than the kernel buffer will pause the streaming, so a return of 0 length is a pause
        // returning less than but still something should call the callback again!

        // with no length, it makes sense to use chunked transfer? keep alive

        // if we have 0 length we fall back to connection close!
        // return uWS::HTTP_STREAM_FIN to signal closure



        // flow below should be:

        // 1. pull a chunk
        // 2. check for signals in this chunk and break appropriately
        // 3. if no signals, send this chunk
        // 4. if sent fine but still has data to write, goto 1
        // 5. if sent less than expected, set offset and callback to httpResponseData and exit

        int offset = 0;
        while (true) {
            // pull chunk
            auto [msg_more, chunk] = cb(offset);

            // check for signals
            if (chunk.length() == 0) {
                if (chunk == HTTP_STREAM_FIN.second) {
                    // flush and FIN
                    AsyncSocket<SSL>::uncork();
                    us_socket_shutdown((us_socket *) this);
                    return;
                } else {
                    std::cout << "Paused stream!" << std::endl;
                }

                // skip sending optional, yet keep refusing to call onWritable while in paused mode (SSL may poll for writable!)
                // we thus need a status: paused to check for before requesting more data (also check for this in resume call!)
                return;
            }

            // send this chunk
            int written = AsyncSocket<SSL>::write(chunk.data(), chunk.length(), true);

            // if we sent less than expected, always end
            if (written < chunk.length()) {
                std::cout << "HttpResponse::write failed to write everything" << std::endl;
                httpResponseData->offset = offset + written;
                httpResponseData->outStream = cb;
            }

            // we have more to send or are still not FINed
            if (length == 0 || offset + written < length) {
                offset += written;
                continue;
            }

            // when do we get here?
            std::cout << "How did we get here?" << std::endl;
            break;
        }


    }

    /* Convenience function for static data */
    void write(std::string_view data, std::function<void(std::string_view)> cb = nullptr) {
        if (cb) {
            // todo: think about how the stream will signal done (streams API challenge overall)
            write([data](int offset) {

                // if offset == length then we know it is end

                // what if we want to return both fin and data? can't do that



                return std::make_pair<bool, std::string_view>(false, data.substr(offset));//{false, data.substr(offset)};
            }, data.length());
        } else {
            // requires no extra alloc
            write([data](int offset) {
                return std::make_pair<bool, std::string_view>(false, data.substr(offset));//{false, data.substr(offset)};
            }, data.length());
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
