#ifndef APP_H
#define APP_H

/* An app is a convenience wrapper of some of the most used fuctionalities and allows a
 * builder-pattern kind of init. Apps operate on the implicit thread local Loop */

#include "HttpContext.h"
#include "HttpResponse.h"

namespace uWS {
template <bool SSL>
struct TemplatedApp {
private:
    HttpContext<SSL> *httpContext;

public:

    ~TemplatedApp() {

    }

    TemplatedApp(const TemplatedApp &other) {
        httpContext = other.httpContext;
    }

    TemplatedApp(us_ssl_socket_context_options sslOptions) {
        httpContext = uWS::HttpContext<SSL>::create(uWS::Loop::defaultLoop(), &sslOptions);
    }

    TemplatedApp &get(std::string pattern, std::function<void(HttpResponse<SSL> *, HttpRequest *)> handler) {
        httpContext->onGet(pattern, handler);
        return *this;
    }

    TemplatedApp &listen(int port, std::function<void(us_listen_socket *)> handler) {
        handler(httpContext->listen(nullptr, port, 0));
        return *this;
    }

    TemplatedApp &run() {
        uWS::run();
        return *this;
    }

};

typedef TemplatedApp<false> App;
typedef TemplatedApp<true> SSLApp;

}

#endif // APP_H
