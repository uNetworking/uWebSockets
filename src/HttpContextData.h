#ifndef HTTPCONTEXTDATA_H
#define HTTPCONTEXTDATA_H

#include "HttpRouter.h"

#include <functional>

namespace uWS {
template<bool> struct HttpResponse;
struct HttpRequest;

template <bool SSL>
struct HttpContextData {
    template <bool> friend struct HttpContext;
private:
    struct RouterData {
        HttpResponse<SSL> *httpResponse;
        HttpRequest *httpRequest;
    };

    HttpRouter<RouterData> router;
};

}

#endif // HTTPCONTEXTDATA_H
