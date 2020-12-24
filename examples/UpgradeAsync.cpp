/* We simply call the root header file "App.h", giving you uWS::App and uWS::SSLApp */
#include "App.h"
#include <thread>

/* This is a simple WebSocket "async" upgrade example.
 * You may compile it with "WITH_OPENSSL=1 make" or with "make" */

int main() {
    /* ws->getUserData returns one of these */
    struct PerSocketData {
        /* Define your user data */
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
        .maxBackpressure = 1 * 1024 * 1024,
        /* Handlers */
        .upgrade = [](auto *res, auto *req, auto *context) {

            /* HttpRequest (req) is only valid in this very callback, so we must COPY the headers
             * we need later on while upgrading to WebSocket. You must not access req after first return.
             * Here we create a heap allocated struct holding everything we will need later on. */

            struct UpgradeData {
                std::string secWebSocketKey;
                std::string secWebSocketProtocol;
                std::string secWebSocketExtensions;
                struct us_socket_context_t *context;
                decltype(res) httpRes;
                bool aborted = false;
            } *upgradeData = new UpgradeData {
                std::string(req->getHeader("sec-websocket-key")),
                std::string(req->getHeader("sec-websocket-protocol")),
                std::string(req->getHeader("sec-websocket-extensions")),
                context,
                res
            };

            /* We have to attach an abort handler for us to be aware
             * of disconnections while we perform async tasks */
            res->onAborted([=]() {
                /* We don't implement any kind of cancellation here,
                 * so simply flag us as aborted */
                upgradeData->aborted = true;
                std::cout << "HTTP socket was closed before we upgraded it!" << std::endl;
            });

            /* Simulate some work for 5 seconds. Don't do this in production, use a thread pool.
             * Making a new thread per request and then destroying it is inefficient. */
            auto loop = uWS::Loop::get();
            std::thread thread([=]() {
                std::cout << "Thread " << std::this_thread::get_id() << " handling upgrade" << std::endl;

                /* We could check for early abortions before running
                 * our async task if the task is very expensive */
                std::this_thread::sleep_for(std::chrono::seconds(5));

                /* Did the client abort before our async task finished? If not, upgrade them! */
                if (!upgradeData->aborted) {
                    std::cout << "Async task done, upgrading to WebSocket now!" << std::endl;

                    /* uWS::Loop::defer() is the only thread-safe function!
                     * We must wrap any calls inside this lambda and post execution to the loop thread */
                    loop->defer([=]() {
                        /* If you don't want to upgrade you can instead respond with custom HTTP here,
                        * such as res->writeStatus(...)->writeHeader(...)->end(...); or similar.*/

                        /* This call will immediately emit .open event */
                        upgradeData->httpRes->template upgrade<PerSocketData>({
                            /* We initialize PerSocketData struct here */
                            .something = 13
                        }, upgradeData->secWebSocketKey,
                            upgradeData->secWebSocketProtocol,
                            upgradeData->secWebSocketExtensions,
                            upgradeData->context);
                    });
                } else {
                    std::cout << "Async task done, but the HTTP socket was closed. Skipping upgrade to WebSocket!" << std::endl;
                }

                delete upgradeData;
            });
            thread.detach();

        },
        .open = [](auto *ws) {
            /* Open event here, you may access ws->getUserData() which points to a PerSocketData struct.
             * Here we simply validate that indeed, something == 13 as set in upgrade handler. */
            std::cout << "Something is: " << static_cast<PerSocketData *>(ws->getUserData())->something << std::endl;
        },
        .message = [](auto *ws, std::string_view message, uWS::OpCode opCode) {
            /* We simply echo whatever data we get */
            ws->send(message, opCode);
        },
        .drain = [](auto */*ws*/) {
            /* Check ws->getBufferedAmount() here */
        },
        .ping = [](auto */*ws*/) {
            /* You don't need to handle this one, we automatically respond to pings as per standard */
        },
        .pong = [](auto */*ws*/) {
            /* You don't need to handle this one either */
        },
        .close = [](auto */*ws*/, int /*code*/, std::string_view /*message*/) {
            /* You may access ws->getUserData() here, but sending or
             * doing any kind of I/O with the socket is not valid. */
        }
    }).listen(9001, [](auto *listen_socket) {
        if (listen_socket) {
            std::cout << "Listening on port " << 9001 << std::endl;
        }
    }).run();
}
