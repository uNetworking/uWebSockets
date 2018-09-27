#include "App.h"

#include "examples/helpers/AsyncFileReader.h"

AsyncFileReader asyncFileReader("/home/alexhultman/sintel_small.mp4");


inline std::string slurp(const std::string &path) {
  std::ostringstream buf;
  std::ifstream input (path.c_str());
  buf << input.rdbuf();
  return buf.str();
}

std::string sintelMovie = slurp("/home/alexhultman/sintel_small.mp4");

int main(int argc, char **argv) {

    std::cout << "Sintel movie is " << sintelMovie.length() << " bytes" << std::endl;

    // läs in hela sintel-filmen här, testa strömmarna med den sen!

    uWS::/*SSL*/App(/*{
        .key_file_name = "/home/alexhultman/uWebSockets/misc/ssl/key.pem",
        .cert_file_name = "/home/alexhultman/uWebSockets/misc/ssl/cert.pem",
        .dh_params_file_name = "/home/alexhultman/dhparams.pem",
        .passphrase = "1234"
    }*/).get("/*", [](auto *res, auto *req) {

        //res->writeStatus(uWS::HTTP_200_OK);
        //res->writeHeader("Content-Type", "text/html;charset=utf-8");

        std::cout << "Endar nu" << std::endl;
        // buffer it up and end by draining it
        res->end(sintelMovie);

        // stream it here
        /*if (!res->tryEnd(sintelMovie)) {
            res->onWritable([res](int offset) {
                std::cout << "Streaming data at offset " << offset << std::endl;
                res->tryEnd(std::string_view(sintelMovie).substr(offset));
            });
        }*/

        // tryWrite / tryEnd

        // vad om man skriver tryEnd("<h1>Hallå!</h1>Din user-agent är: ", totalLength)

        //res->write("<h1>Hallå!</h1>Din user-agent är: ");
        //res->end(req->getHeader("user-agent"));

    }).get("/async/sintel.mkv", [](auto *res, auto *req) {

        // I guess it should return wherer or not it wants to be called again?
        auto streamIt = [res](int offset) {
            while(true) {
                /* Peek from cache */
                std::string_view chunk = asyncFileReader.peek(offset);
                if (chunk.length()) {
                    /* We had parts of this file cached already */
                    res->tryEnd(chunk, asyncFileReader.getFileSize());
                } else {
                    /* We had nothing readily available right now, request async chunk and pause the stream until we have */
                    asyncFileReader.request(offset, [res](std::string_view chunk) {
                        /* We were aborted */
                        if (!chunk.length()) {
                            std::cout << "Async File Read request was aborted!" << std::endl;
                            // close the socket here?
                            // we need a way to NOT resume a paused socket! essentially close!
                        } else {
                            /* We finally got the data, resume stream with this chunk */
                            res->tryEnd(chunk, asyncFileReader.getFileSize());
                        }
                    });
                }
                return true;
            }
        };

        // basically do this
        if (!streamIt(0)) {
            // what should it return? will onWritable be called again depending on what it returns?
            res->onWritable(streamIt);
        }

    }).listen(3000, [](auto *token) {
        if (token) {
            std::cout << "Listening on port " << 3000 << std::endl;
        }
    }).run();

}
