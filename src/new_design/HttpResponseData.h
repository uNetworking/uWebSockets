#ifndef HTTPRESPONSEDATA_H
#define HTTPRESPONSEDATA_H

#include "HttpParser.h"
#include <functional>

namespace uWS {

template <bool SSL>
struct HttpResponseData : HttpParser {

    // inStream, outStream
    std::function<void(std::string_view)> inStream;
    std::function<std::string_view(int)> outStream;

    int offset = 0;
    // writeHandler

};

}

#endif // HTTPRESPONSEDATA_H
