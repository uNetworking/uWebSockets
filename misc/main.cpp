#include "App.h"

//#include "../examples/helpers/AsyncFileReader.h"
//#include "../examples/helpers/AsyncFileStreamer.h"

us_listen_socket *token;

int main(int argc, char **argv) {

    struct PerSocketData {
        char pad[256];
        int hello;
    };

    auto app = uWS::/*SSL*/App({
        .key_file_name = "/home/alexhultman/key.pem",
        .cert_file_name = "/home/alexhultman/cert.pem",
        .passphrase = "1234"
    }).any("/anything", [](auto *res, auto *req) {
        std::cout << "Any route with method: " << req->getMethod() << std::endl;
        res->end("Hello Any route!");
    }).get("/exit", [](auto *res, auto *req) {

        if (!token) {
            res->end("Server already closed down!");
            return;
        }

        res->end("Closing down server now");

        /* Use this route to signal stop listening */
        us_listen_socket_close(token);
        token = nullptr;
    }).ws<PerSocketData>("/*", {
        /* Settings */
        .compression = uWS::DEDICATED_COMPRESSOR,
        .maxPayloadLength = 16 * 1024 * 1024,
        .idleTimeout = 10,
        /* Handlers */
        .open = [](auto *ws) {
            std::cout << "WebSocket connected" << std::endl;
            /* Access per socket data */
            PerSocketData *perSocketData = (PerSocketData *) ws->getUserData();
            perSocketData->hello = 13;
        },
        .message = [](auto *ws, std::string_view message, uWS::OpCode opCode) {
            ws->send(message, opCode, true);
            PerSocketData *perSocketData = (PerSocketData *) ws->getUserData();
            std::cout << "OK per socket data: " << (perSocketData->hello == 13) << std::endl;
        },
        .drain = [](auto *ws) {
            std::cout << "Drainage: " << ws->getBufferedAmount() << std::endl;
        },
        .ping = [](auto *ws) {
            std::cout << "Ping" << std::endl;
        },
        .pong = [](auto *ws) {
            std::cout << "Pong" << std::endl;
        },
        .close = [](auto *ws, int code, std::string_view message) {
            std::cout << "WebSocket disconnected: " << code << "[" << message << "]" << std::endl;
            /* Access per socket data */
            PerSocketData *perSocketData = (PerSocketData *) ws->getUserData();
            std::cout << "OK per socket data: " << (perSocketData->hello == 13) << std::endl;
        }
    }).listen(9001, [](auto *token) {
        ::token = token;
        if (token) {
            std::cout << "Listening on port " << 3000 << std::endl;
        }
    }).run();


    std::cout << "Everything fine, falling through" << std::endl;

//    return 0;

//    AsyncFileStreamer *asyncFileStreamer = new AsyncFileStreamer("/home/alexhultman/v0.15/public");

//    uWS::/*SSL*/App(/*{
//        .key_file_name = "/home/alexhultman/uWebSockets/misc/ssl/key.pem",
//        .cert_file_name = "/home/alexhultman/uWebSockets/misc/ssl/cert.pem",
//        .dh_params_file_name = "/home/alexhultman/dhparams.pem",
//        .passphrase = "1234"
//    }*/)/*.get("/*", [](auto *res, auto *req) {

//        res->end("GET /WILDCARD");

//    })*/.get("/:param1/:param2", [](auto *res, auto *req) {

//        res->write("GET /:param1/:param2 = ");
//        res->write(req->getParameter(0));
//        res->write(" and ");
//        res->end(req->getParameter(1));

//    }).post("/hello", [asyncFileStreamer](auto *res, auto *req) {

//        // depending on the file type we want to also add mime!
//        //asyncFileStreamer->streamFile(res, req->getUrl());

//        res->end("POST /hello");

//    }).get("/hello", [](auto *res, auto *req) {

//        res->end("GET /hello");

//    }).unhandled([](auto *res, auto *req) {

//        res->writeStatus("404 Not Found");
//        res->writeHeader("Content-Type", "text/html; charset=utf-8");
//        res->end("<h1>404 Not Found</h1><i>µWebSockets v0.15</i>");

//    }).listen(3000, [](auto *token) {
//        if (token) {
//            std::cout << "Listening on port " << 3000 << std::endl;
//        }
//    }).run();

}
