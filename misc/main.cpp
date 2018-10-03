#include "App.h"

#include "../examples/helpers/AsyncFileReader.h"
#include "../examples/helpers/AsyncFileStreamer.h"

int main(int argc, char **argv) {

    AsyncFileStreamer *asyncFileStreamer = new AsyncFileStreamer("/home/alexhultman/v0.15/public");

    uWS::/*SSL*/App(/*{
        .key_file_name = "/home/alexhultman/uWebSockets/misc/ssl/key.pem",
        .cert_file_name = "/home/alexhultman/uWebSockets/misc/ssl/cert.pem",
        .dh_params_file_name = "/home/alexhultman/dhparams.pem",
        .passphrase = "1234"
    }*/).get("/*", [](auto *res, auto *req) {

        res->end("GET /WILDCARD");

    }).post("/hello", [asyncFileStreamer](auto *res, auto *req) {

        // depending on the file type we want to also add mime!
        //asyncFileStreamer->streamFile(res, req->getUrl());

        res->end("POST /hello");

    }).get("/hello", [](auto *res, auto *req) {

        res->end("GET /hello");

    }).unhandled([](auto *res, auto *req) {

        res->end("Here's nothing for you to see!");

    }).listen(3000, [](auto *token) {
        if (token) {
            std::cout << "Listening on port " << 3000 << std::endl;
        }
    }).run();

}
