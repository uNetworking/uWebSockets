#ifndef HTTPRESPONSEDATA_H
#define HTTPRESPONSEDATA_H

// so what do we depend on?

#include <functional>

template <bool SSL>
struct HttpResponseData {

    std::function<void(std::string_view)> readHandler;

};

#endif // HTTPRESPONSEDATA_H
