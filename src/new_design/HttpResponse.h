#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H

#include "AsyncSocket.h"
#include "HttpResponseData.h"

namespace uWS {

const char *HTTP_200_OK = "200 OK";

template <bool SSL>
struct HttpResponse : public AsyncSocket<SSL> {
private:

    using SOCKET_TYPE = typename StaticDispatch<SSL>::SOCKET_TYPE;
    using StaticDispatch<SSL>::static_dispatch;

    HttpResponseData<SSL> *getHttpResponseData() {
        return (HttpResponseData<SSL> *) static_dispatch(us_ssl_socket_ext, us_socket_ext)((SOCKET_TYPE *) this);
    }

public:

    HttpResponse *writeStatus(std::string_view status) {
        AsyncSocket<SSL>::write("HTTP/1.1 ", 9);
        AsyncSocket<SSL>::write(status.data(), status.length());
        AsyncSocket<SSL>::write("\r\n", 2);
        return this;
    }

    HttpResponse *writeHeader(std::string_view key, std::string_view value) {
        AsyncSocket<SSL>::write(key.data(), key.length());
        AsyncSocket<SSL>::write(": ", 2);
        AsyncSocket<SSL>::write(value.data(), value.length());
        AsyncSocket<SSL>::write("\r\n", 2);
        return this;
    }

    void write(std::function<std::string_view(int)> cb, int length) {
        // what if the streamer cannot return any data?
        // then it should return something to pause write, and then start it again
        // basically we need throttling
        std::string_view chunk = cb(0);

        AsyncSocket<SSL>::write("Content-Length: ", 16);
        AsyncSocket<SSL>::writeUnsigned(chunk.length());
        AsyncSocket<SSL>::write("\r\n\r\n", 4);
        if (AsyncSocket<SSL>::writeOptionally(chunk.data(), chunk.length()) < length) {
            getHttpResponseData()->outStream = cb;
        }
    }

    // this will probably not be this clean: it will most probably want to do some active pulling of data?
    void read(std::function<void(std::string_view)> handler) {
        HttpResponseData<SSL> *data = getHttpResponseData();

        data->inStream = handler;
    }

    // this should not be anything other than a simple convenience wrapper of streams!
    void end(std::string_view data) {
        writeStatus("200 OK")->write([data](int offset) {
            return std::string_view(data.data() + offset, data.length() - offset);
        }, data.length());
    }
};

}

#endif // HTTPRESPONSE_H
