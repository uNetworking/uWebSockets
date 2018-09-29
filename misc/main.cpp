#include "App.h"

#include "../examples/helpers/AsyncFileReader.h"

AsyncFileReader asyncFileReader("/home/alexhultman/sintel_small.mp4");

/*inline std::string slurp(const std::string &path) {
  std::ostringstream buf;
  std::ifstream input (path.c_str());
  buf << input.rdbuf();
  return buf.str();
}

std::string sintelMovie = slurp("/home/alexhultman/sintel_small.mp4");*/

void streamFile(uWS::HttpResponse<false> *res) {
    //int offset = res->getWriteOffset();

    std::cout << "streamFile called with offset: " << res->getWriteOffset() << std::endl;

    /* Peek from cache */
    std::string_view chunk = asyncFileReader.peek(res->getWriteOffset());
    if (!chunk.length() || res->tryEnd(chunk, asyncFileReader.getFileSize())) {
        // request new chunk
        std::cout << "Requesting new chunk!" << std::endl;

        asyncFileReader.request(res->getWriteOffset(), [res](std::string_view chunk) {
            /* We were aborted */
            if (!chunk.length()) {
                std::cout << "Async File Read request was aborted!" << std::endl;
                // close the socket here?
                // we need a way to NOT resume a paused socket! essentially close!
            } else {

                // just call streamIt!
                std::cout << "Got requested data, calling streamFile now!" << std::endl;
                streamFile(res);
            }
        });
    } else {
        std::cout << "Could not write everything, setting onWritable!" << std::endl;
        // we have chunk but we could not write everything, so retry this in onWritable
        res->onWritable([res](int offset) {
            std::cout << "We are writable now, calling StreamFile" << std::endl;
            streamFile(res);
            return false;
        });
    }
}

int main(int argc, char **argv) {

    //std::cout << "Sintel movie is " << sintelMovie.length() << " bytes" << std::endl;

    // läs in hela sintel-filmen här, testa strömmarna med den sen!

    uWS::/*SSL*/App(/*{
        .key_file_name = "/home/alexhultman/uWebSockets/misc/ssl/key.pem",
        .cert_file_name = "/home/alexhultman/uWebSockets/misc/ssl/cert.pem",
        .dh_params_file_name = "/home/alexhultman/dhparams.pem",
        .passphrase = "1234"
    }*/).get("/", [](auto *res, auto *req) {

        res->writeStatus(uWS::HTTP_200_OK);
        res->writeHeader("Content-Type", "text/html;charset=utf-8");

        // buffer it up and end by draining it
        //res->end(sintelMovie);

        // stream it here
        /*if (!res->tryEnd(sintelMovie)) {
            res->onWritable([res](int offset) {
                return res->tryEnd(std::string_view(sintelMovie).substr(offset));
            })->onAborted([]() {
                // was it really aborted?
                std::cout << "Streaming was aborted" << std::endl;
            });
        }*/

        // end can be called two times if the total length is longer then given
        res->write("<h1>Hallå!</h1>Din user-agent är: ");
        res->end(req->getHeader("user-agent"));

    }).get("/async/sintel.mkv", [](auto *res, auto *req) {

        // this should be enough, we can even delay this thing!
        streamFile(res);

    }).listen(3000, [](auto *token) {
        if (token) {
            std::cout << "Listening on port " << 3000 << std::endl;
        }
    }).run();

}
