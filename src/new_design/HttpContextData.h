#ifndef HTTPCONTEXTDATA_H
#define HTTPCONTEXTDATA_H

// we depend on these
#include "HttpParser.h"
#include "HttpRouter.h"

#include <functional>

namespace uWS {
template<bool>
struct HttpResponse;
}


template <bool SSL>
struct HttpContextData {

private:

public:

    struct HttpRouterUserData {

    };

    HttpRouter<HttpRouterUserData> httpRouter;

    // placeholder handler for response
    std::function<void(uWS::HttpResponse<SSL> *, HttpRequest *)> handler;

};

#endif // HTTPCONTEXTDATA_H
