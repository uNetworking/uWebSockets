#ifndef HTTPCONTEXTDATA_H
#define HTTPCONTEXTDATA_H

#include "HttpRouter.h"

#include <functional>

namespace uWS {
template<bool> struct HttpResponse;
struct HttpRequest;

template <bool SSL>
struct HttpContextData {

private:

public:

    struct UserData {
        HttpResponse<SSL> *httpResponse;
        HttpRequest *httpRequest;
    };

    HttpRouter<UserData *> router;

};

}

#endif // HTTPCONTEXTDATA_H
