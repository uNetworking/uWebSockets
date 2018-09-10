#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H

#include "HttpResponseData.h"

// we will most probably depend on the LoopData to do corking and such

namespace uWS {

template <bool SSL>
struct HttpResponse {
private:

    // helpers
    HttpResponseData<SSL> *getHttpResponseData() {

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
