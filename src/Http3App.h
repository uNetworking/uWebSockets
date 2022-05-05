#include "App.h"

#include "Http3Response.h"
#include "Http3Request.h"
#include "Http3Context.h"

namespace uWS {

    struct QuicApp {
        Http3Context *http3Context;

        QuicApp(SocketContextOptions options = {}) {
            /* Create the http3 context */
            http3Context = Http3Context::create((us_loop_t *)Loop::get(), {});
        }

        QuicApp &listen(int port, std::function<void(void *)> cb) {
            return *this;
        }

        QuicApp &get(std::string path, std::function<void(Http3Response *, Http3Request *)> cb) {
            // http3Context->onHttp, internally using httprouter

            return *this;
        }

        void run() {
            uWS::Loop::get()->run();
        }
    };


}