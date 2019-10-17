#include "App.h"

struct us_listen_socket_t *listen_socket;

int main() {
    /* ws->getUserData returns one of these */
    struct PerSocketData {

    };

    /* Very simple WebSocket broadcasting echo server */
    uWS::App().ws<PerSocketData>("/*", {
        /* Settings */
        .compression = uWS::SHARED_COMPRESSOR,
        .maxPayloadLength = 16 * 1024 * 1024,
        .idleTimeout = 10,
        .maxBackpressure = 1 * 1024 * 1204,
        /* Handlers */
        .open = [](auto *ws, auto *req) {
            /* Let's make every connection subscribe to the "broadcast" topic */
            ws->subscribe("broadcast");
        },
        .message = [](auto *ws, std::string_view message, uWS::OpCode opCode) {
            /* Exit gracefully if we get a closedown message (ASAN debug) */
            if (message == "closedown") {
               /* Bye bye */
               us_listen_socket_close(0, listen_socket);
               ws->close();
            }

            /* Simply broadcast every single message we get */
            ws->publish("broadcast", message, opCode);
        },
        .drain = [](auto *ws) {
            /* Check getBufferedAmount here */
        },
        .ping = [](auto *ws) {

        },
        .pong = [](auto *ws) {

        },
        .close = [](auto *ws, int code, std::string_view message) {
            /* We automatically unsubscribe from any topic here */
        }
    }).listen(9001, [](auto *token) {
        listen_socket = token;
        if (token) {
            std::cout << "Listening on port " << 9001 << std::endl;
        }
    }).run();
}
