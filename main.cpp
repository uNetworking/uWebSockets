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
        res->writeStatus(uWS::HTTP_200_OK)->write("Hello world!");
    }).get("/endless", [](auto *res, auto *req) {
        /* This route doesn't specify any length and only returns 1 char per chunk */
        res->write([](int offset) {
            std::string_view data("<html><body><h1>Hello, world</h1></body></html>");
            if (offset < data.length()) {
                return std::make_pair<bool, std::string_view>(offset < data.length() - 1, data.substr(offset, 1));
            }
            return uWS::HTTP_STREAM_FIN;
        });
    }).get("/async/sintel.mkv", [](auto *res, auto *req) {

        // res->write(asyncFileReader.stream(res))
        // asyncFileReader.getAsStream()

        /* This route streams back chunks of data in delayed fashion */
        res->writeStatus(uWS::HTTP_200_OK)->write([res](int offset) {

            /* Handle broken stream */
            if (offset == -1) {
                std::cout << "Stream was closed by peer!" << std::endl;
                return uWS::HTTP_STREAM_IGNORE;
            }

            /* Peek from cache */
            std::string_view chunk = asyncFileReader.peek(offset);
            if (chunk.length()) {
                /* We had parts of this file cached already */
                return std::pair<bool, std::string_view>(false, chunk);
            } else {

                //std::string_view outerChunk;

                /* We had nothing readily available right now, request async chunk and pause the stream until we have */
                asyncFileReader.request(offset, [res](std::string_view chunk) {

                    std::cout << "We came here!" << std::endl;

                    /* We were aborted */
                    if (!chunk.length()) {
                        std::cout << "Async File Read request was aborted!" << std::endl;
                        // close the socket here?
                        // we need a way to NOT resume a paused socket! essentially close!
                    } else {
                        /* We finally got the data, resume stream with this chunk */
                        res->resume(chunk);
                    }
                });

                //std::cout << "Returning chunk of size: " << outerChunk.length() << std::endl;
                //return std::pair<bool, std::string_view>(false, outerChunk);

                // what if we resumed before we paused! we cannot do that!

                std::cout << "PAusing stream out due to empty cache!" << std::endl;
                return uWS::HTTP_STREAM_PAUSE;
            }
        }, asyncFileReader.getFileSize());
    }).listen(3000, [](auto *token) {
        if (token) {
            std::cout << "Listening on port " << 3000 << std::endl;
        }
    }).run();

}
