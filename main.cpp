#include "App.h"

#include "examples/helpers/AsyncFileReader.h"

AsyncFileReader asyncFileReader("/home/alexhultman/v0.15/sintel.mkv");

int main(int argc, char **argv) {

    uWS::/*SSL*/App(/*{
        .key_file_name = "/home/alexhultman/uWebSockets/misc/ssl/key.pem",
        .cert_file_name = "/home/alexhultman/uWebSockets/misc/ssl/cert.pem",
        .dh_params_file_name = "/home/alexhultman/dhparams.pem",
        .passphrase = "1234"
    }*/).get("/", [](auto *res, auto *req) {

        res->writeStatus(uWS::HTTP_200_OK);
        res->writeHeader("Content-Type", "text/html;charset=utf-8");

        res->write("<h1>Hallå!</h1>Din user-agent är: ");
        res->end(req->getHeader("user-agent"));

    }).get("/async/sintel.mkv", [](auto *res, auto *req) {

        // asyncFileReader.getFileSize()

        /* Peek from cache */
        std::string_view chunk = asyncFileReader.peek(/*offset*/ 0);
        if (chunk.length()) {
            /* We had parts of this file cached already */
            res->write(chunk);
        } else {
            /* We had nothing readily available right now, request async chunk and pause the stream until we have */
            asyncFileReader.request(/*offset*/ 0, [res](std::string_view chunk) {
                /* We were aborted */
                if (!chunk.length()) {
                    std::cout << "Async File Read request was aborted!" << std::endl;
                    // close the socket here?
                    // we need a way to NOT resume a paused socket! essentially close!
                } else {
                    /* We finally got the data, resume stream with this chunk */
                    res->write(chunk);
                }
            });
        }

    }).listen(3000, [](auto *token) {
        if (token) {
            std::cout << "Listening on port " << 3000 << std::endl;
        }
    }).run();

}
