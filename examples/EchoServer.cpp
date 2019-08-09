#include "App.h"

int main() {
    /* ws->getUserData returns one of these */
    struct PerSocketData {

    };

    /* Very simple WebSocket echo server */
    uWS::App().ws<PerSocketData>("/*", {
        /* Settings */
        .compression = uWS::SHARED_COMPRESSOR,
        .maxPayloadLength = 16 * 1024,
	.idleTimeout = 10,
        /* Handlers */
        .open = [](auto *ws, auto *req) {

        },
        .message = [](auto *ws, std::string_view message, uWS::OpCode opCode) {
            ws->send(message, opCode);
        },
        .drain = [](auto *ws) {
            /* Check getBufferedAmount here */
        },
        .ping = [](auto *ws) {

        },
        .pong = [](auto *ws) {

        },
        .close = [](auto *ws, int code, std::string_view message) {

        }
    }).listen(9001, [](auto *token) {
        if (token) {
            std::cout << "Listening on port " << 9001 << std::endl;
        }
    }).run();
}
