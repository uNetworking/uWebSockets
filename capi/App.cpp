#include "libuwebsockets.h"
#include "App.h"

extern "C" {

    uws_app_t *uws_create_app() {
        return (uws_app_t *) new uWS::App();
    }

    void uws_app_get(uws_app_t *app, const char *pattern, void (*handler)(uws_res_t *, uws_req_t *)) {
        uWS::App *uwsApp = (uWS::App *) app;
        uwsApp->get(pattern, [handler](auto *res, auto *req) {
            handler((uws_res_t *) res, (uws_req_t *) req);
        });
    }

    void uws_app_run(uws_app_t *app) {
        uWS::App *uwsApp = (uWS::App *) app;
        uwsApp->run();
    }

    void uws_res_end(uws_res_t *res, const char *data, size_t length) {
        uWS::HttpResponse<false> *uwsRes = (uWS::HttpResponse<false> *) res;
        uwsRes->end(data, length);
    }

    void uws_app_listen(uws_app_t *app, int port, void (*handler)(void *)) {
        uWS::App *uwsApp = (uWS::App *) app;
        uwsApp->listen(port, [handler](struct us_listen_socket_t *listen_socket) {
            handler((void *) listen_socket);
        });
    }

}