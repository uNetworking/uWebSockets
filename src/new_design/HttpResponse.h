#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H

#include "HttpResponseData.h"
#include "StaticDispatch.h"

// we will most probably depend on the LoopData to do corking and such

namespace uWS {

template <bool SSL>
struct HttpResponse : StaticDispatch<SSL> {
private:

    using SOCKET_TYPE = typename StaticDispatch<SSL>::SOCKET_TYPE;
    using StaticDispatch<SSL>::static_dispatch;

    // helpers
    HttpResponseData<SSL> *getHttpResponseData() {
        return (HttpResponseData<SSL> *) static_dispatch(us_ssl_socket_ext, us_socket_ext)((SOCKET_TYPE *) this);
    }

public:

    void writeHeader() {

    }

    // this will probably not be this clean: it will most probably want to do some active pulling of data?
    void read(std::function<void(std::string_view)> handler) {
        HttpResponseData<SSL> *data = getHttpResponseData();

        data->readHandler = handler;
    }

    // read and write streams most definitely will be called by the context, thus the context absolutely depends on the httpresponse!
    // or, we both depend on each others data structures only?
};

}

#endif // HTTPRESPONSE_H
