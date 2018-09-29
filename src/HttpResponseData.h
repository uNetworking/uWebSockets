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
        HTTP_STATUS_CALLED = 1, // used
        HTTP_WRITE_CALLED = 2, // used
        HTTP_END_CALLED = 4, // used
        HTTP_PAUSED_STREAM_OUT = 8, // not used
        HTTP_ENDED_STREAM_OUT = 16 // not used
    };

    /* Per socket event handlers */
    std::function<bool(int)> onWritable;
    std::function<void()> onAborted;
    //std::function<void()> onData;

    std::function<void(std::string_view)> inStream;
    std::function<std::pair<bool, std::string_view>(int)> outStream;
    /* Outgoing offset */
    int offset = 0;

    /* Current state (content-length sent, status sent, write called, etc */
    int state = 0;
};

}

#endif // HTTPRESPONSEDATA_H
