#ifndef HTTPRESPONSEDATA_H
#define HTTPRESPONSEDATA_H

/* This data belongs to the HttpResponse */

#include "HttpParser.h"
#include "AsyncSocketData.h"
#include <functional>

namespace uWS {

template <bool SSL>
struct HttpResponseData : HttpParser, AsyncSocketData<SSL> {
    std::function<void(std::string_view)> inStream;
    std::function<std::string_view(int)> outStream;
    /* Outgoing offset */
    int offset = 0;
};

}

#endif // HTTPRESPONSEDATA_H
