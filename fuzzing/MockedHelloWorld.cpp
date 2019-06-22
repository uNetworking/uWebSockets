#include "App.h"

#include "helpers.h"

/* This function pushes data to the uSockets mock */
extern "C" void us_loop_read_mocked_data(struct us_loop *loop, char *data, unsigned int size);

uWS::TemplatedApp<false> *app;
us_listen_socket_t *listenSocket;

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv) {

    app = new uWS::TemplatedApp<false>(uWS::App().get("/*", [](auto *res, auto *req) {
        if (req->getHeader("use_write").length()) {
            res->writeStatus("200 OK")->writeHeader("write", "true")->write("Hello");
            res->write(" world!");
            res->end();
        } else if (req->getQuery().length()) {
            res->close();
        } else {
            res->end("Hello world!");
        }
    }).post("/*", [](auto *res, auto *req) {
        res->onAborted([]() {

        });
        res->onData([res](std::string_view chunk, bool isEnd) {
            if (isEnd) {
                res->end(chunk);
            }
        });
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
