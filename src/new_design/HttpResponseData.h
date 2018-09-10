#ifndef HTTPRESPONSEDATA_H
#define HTTPRESPONSEDATA_H

// so what do we depend on?

#include "HttpParser.h"
#include <functional>

namespace uWS {

template <bool SSL>
struct HttpResponseData : HttpParser {

    std::function<void(std::string_view)> readHandler;

};

}

#endif // HTTPRESPONSEDATA_H
