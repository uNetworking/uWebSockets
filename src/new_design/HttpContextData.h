#ifndef HTTPCONTEXTDATA_H
#define HTTPCONTEXTDATA_H

// this means we will depend on HttpRouter and HttpParser here!
#include "../http/HttpParser.h"

#include "../http/HttpRouter.h"

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
    std::function<void(uWS::HttpResponse<SSL> *)> handler;

};

#endif // HTTPCONTEXTDATA_H
