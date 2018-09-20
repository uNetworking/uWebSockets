#ifndef HTTPRESPONSEDATA_H
#define HTTPRESPONSEDATA_H

/* This data belongs to the HttpResponse */

#include "HttpParser.h"
#include "AsyncSocketData.h"
#include <functional>

namespace uWS {

template <bool SSL>
struct HttpResponseData : HttpParser, AsyncSocketData<SSL> {
    template <bool> friend struct HttpResponse;
    template <bool> friend struct HttpContext;
private:
    /* Bits of status */
    enum {
        HTTP_STATUS_SENT = 1,
        HTTP_WRITE_CALLED = 2,
        HTTP_KNOWN_STREAM_OUT_SIZE = 4,
        HTTP_PAUSED_STREAM_OUT = 8,
        HTTP_ENDED_STREAM_OUT = 16
    };

    std::function<void(std::string_view)> inStream;
    std::function<std::pair<bool, std::string_view>(int)> outStream;
    /* Outgoing offset */
    int offset = 0;

    /* Current state (content-length sent, status sent, write called, etc */
    int state = 0;
};

}

#endif // HTTPRESPONSEDATA_H
