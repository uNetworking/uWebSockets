#include "App.h"

#include "helpers.h"

/* This function pushes data to the uSockets mock */
extern "C" void us_loop_read_mocked_data(struct us_loop *loop, char *data, unsigned int size);

uWS::TemplatedApp<false> *app;
us_listen_socket_t *listenSocket;

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv) {

    /* ws->getUserData returns one of these */
    struct PerSocketData {
        int nothing;
    };

    /* Very simple WebSocket echo server */
    app = new uWS::TemplatedApp<false>(uWS::App().ws<PerSocketData>("/*", {
        /* Settings */
        .compression = uWS::SHARED_COMPRESSOR,
        /* We want this to be low so that we can hit it, yet bigger than 256 */
        .maxPayloadLength = 300,
        .idleTimeout = 10,
        /* Handlers */
        .open = [](auto *ws, auto *req) {
            if (req->getHeader("close_me").length()) {
                ws->close();
            } else if (req->getHeader("end_me").length()) {
                ws->end(1006);
            }
        },
        .message = [](auto *ws, std::string_view message, uWS::OpCode opCode) {
            if (message.length() > 300) {
                /* Inform the sanitizer of the fault */
                fprintf(stderr, "Too long message passed\n");
                free((void *) -1);
            }

            if (message.length() && message[0] == 'C') {
                ws->close();
            } else if (message.length() && message[0] == 'E') {
                ws->end(1006);
            } else {
                ws->send(message, opCode, true);
            }
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
    }).listen(9001, [](us_listen_socket_t *listenSocket) {
        if (listenSocket) {
            std::cout << "Listening on port " << 9001 << std::endl;
            ::listenSocket = listenSocket;
        }
    }));

    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    us_loop_read_mocked_data((struct us_loop *) uWS::Loop::get(), (char *) makePadded(data, size), size);

    return 0;
}
