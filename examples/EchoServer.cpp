/* We simply call the root header file "App.h", giving you uWS::App and uWS::SSLApp */
#include "App.h"

/* This is a simple WebSocket echo server example.
 * You may compile it with "WITH_OPENSSL=1 make" or with "make" */

int main() {
    /* ws->getUserData returns one of these */
    struct PerSocketData {
        /* Fill with user data */
        int something;
    };

    /* Keep in mind that uWS::SSLApp({options}) is the same as uWS::App() when compiled without SSL support.
     * You may swap to using uWS:App() if you don't need SSL */
    uWS::SSLApp({
        /* There are example certificates in uWebSockets.js repo */
	    .key_file_name = "../misc/key.pem",
	    .cert_file_name = "../misc/cert.pem",
	    .passphrase = "1234"
	}).ws<PerSocketData>("/*", {
        /* Settings */
        .compression = uWS::SHARED_COMPRESSOR,
        .maxPayloadLength = 16 * 1024,
        .idleTimeout = 10,
        .maxBackpressure = 1 * 1024 * 1204,
        /* Handlers */

        //.syncUpgrade()

        .upgrade = [](auto *res, auto *req, auto *context) {

            /* Immediate path */
            res->template upgrade<PerSocketData>(req->getHeader("sec-websocket-key"),
                req->getHeader("sec-websocket-protocol"),
                req->getHeader("sec-websocket-extensions"),
                context);

            return;

            std::cout << "Upgrade request!" << std::endl;

            /* Async path, you have to COPY headers */
            std::string secWebSocketKey(req->getHeader("sec-websocket-key"));
            std::string secWebSocketProtocol(req->getHeader("sec-websocket-protocol"));
            std::string secWebSocketExtensions(req->getHeader("sec-websocket-extensions"));

            /* If the client disconnects inbetween, you MUST avoid upgrading */
            bool *aborted = new bool(false);
            res->onAborted([=]() {
                std::cout << "WebSocket upgrade aborted!" << std::endl;
                *aborted = true;
            });

            /* Simulate checking auth for 10 seconds */
            struct us_loop_t *loop = (struct us_loop_t *) uWS::Loop::get();
            struct us_timer_t *delayTimer = us_create_timer(loop, 0, 0);
            us_timer_set(delayTimer, [](struct us_timer_t *t) {
                std::cout << "Timer triggered!" << std::endl;

                us_timer_close(t);
            }, 5000, 0);

            /* Simulate doing async work by deferring upgrade to next event loop iteration */
            uWS::Loop::get()->defer([=](){

                std::cout << "Upgrading now!" << std::endl;

                if (!*aborted) {

                    // this should get some kind of ticket
                    res->template upgrade<PerSocketData>(secWebSocketKey,
                        secWebSocketProtocol,
                        secWebSocketExtensions,
                        context);
                }

                delete aborted;
            });

            /* Immediate path is in all seriousness a simple return bool */

            // not exactly, you need to pass UserData - return a moved UserData and bool?


        },
        .open = [](auto *ws, auto *req) {
            /* Open event here, you may access ws->getUserData() which points to a PerSocketData struct */
            std::cout << "Something is: " << static_cast<PerSocketData *>(ws->getUserData())->something << std::endl;
        },
        .message = [](auto *ws, std::string_view message, uWS::OpCode opCode) {
            ws->send(message, opCode);
        },
        .drain = [](auto *ws) {
            /* Check ws->getBufferedAmount() here */
        },
        .ping = [](auto *ws) {
            /* Not implemented yet */
        },
        .pong = [](auto *ws) {
            /* Not implemented yet */
        },
        .close = [](auto *ws, int code, std::string_view message) {
            /* You may access ws->getUserData() here */
        }
    }).listen(9001, [](auto *token) {
        if (token) {
            std::cout << "Listening on port " << 9001 << std::endl;
        }
    }).run();
}
