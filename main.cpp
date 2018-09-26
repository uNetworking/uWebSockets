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

//    }).get("/endless", [](auto *res, auto *req) {
//        /* This route doesn't specify any length and only returns 1 char per chunk */
//        res->write([](int offset) {
//            std::string_view data("<html><body><h1>Hello, world</h1></body></html>");
//            if (offset < data.length()) {
//                return std::make_pair<bool, std::string_view>(offset < data.length() - 1, data.substr(offset, 1));
//            }
//            return uWS::HTTP_STREAM_FIN;
//        });
    }).get("/async/sintel.mkv", [](auto *res, auto *req) {

        // res->write(asyncFileReader.stream(res))
        // asyncFileReader.getAsStream()


        // res->write(data, length) -> bool (identical to nodejs)
        // res->tryWrite(data, length) -> int (what we will use mostly)
        // both operate like uSockets, they are paused by default and will trigger onWritable if set

        // res->onWritable(lambda) -> the same as "drain" even in Node.js, call tryWrite here, or do nothing to hang/pause

        // resume is gone, it will resume automatically as needed, just like uSockets do automatically

        // res will be somewhat like AsyncSocket but with certain middle functions for formatting the data (think Content-Length and Chunked)


        // res->onAborted(lambda) will be called when closed, for now all events lie on the res

        // res->onData() will probably be the same?

        //res->writeStatus(uWS::HTTP_200_OK)->end("Hello Sintel!");

        // om vi håller headers i en buffer och data i en annan?

        // om end eller tryEnd inte kommer före write swappar den till chunked!
        res->write("Hallå! Din user-agent är: "); // -> skriver till cork-buffern "Transfer-Encoding: chunked" och ett chunk
        res->end(req->getHeader("user-agent"));

        // write före end går över till chunked, från där kan man extenda till SSE men websockets är bättre format (binärt, mindre)

        // hur fungerar detta med server-sent-events?


        // end efter write blir ett noll-segment i chunked!

        /*if (!res->tryEnd()) {

        }*/



        /* This route streams back chunks of data in delayed fashion */
//        res->writeStatus(uWS::HTTP_200_OK)->write([res](int offset) {

//            /* Handle broken stream */
//            if (offset == -1) {
//                std::cout << "Stream was closed by peer!" << std::endl;
//                return uWS::HTTP_STREAM_IGNORE;
//            }

//            /* Peek from cache */
//            std::string_view chunk = asyncFileReader.peek(offset);
//            if (chunk.length()) {
//                /* We had parts of this file cached already */
//                return std::pair<bool, std::string_view>(false, chunk);
//            } else {

//                //std::string_view outerChunk;

//                /* We had nothing readily available right now, request async chunk and pause the stream until we have */
//                asyncFileReader.request(offset, [res](std::string_view chunk) {

//                    //std::cout << "We came here!" << std::endl;

//                    /* We were aborted */
//                    if (!chunk.length()) {
//                        std::cout << "Async File Read request was aborted!" << std::endl;
//                        // close the socket here?
//                        // we need a way to NOT resume a paused socket! essentially close!
//                    } else {
//                        /* We finally got the data, resume stream with this chunk */
//                        res->resume(chunk);
//                    }
//                });

//                //std::cout << "Returning chunk of size: " << outerChunk.length() << std::endl;
//                //return std::pair<bool, std::string_view>(false, outerChunk);

//                // what if we resumed before we paused! we cannot do that!

//                //std::cout << "PAusing stream out due to empty cache!" << std::endl;
//                return uWS::HTTP_STREAM_PAUSE;
//            }
//        }, asyncFileReader.getFileSize());
    }).listen(3000, [](auto *token) {
        if (token) {
            std::cout << "Listening on port " << 3000 << std::endl;
        }
    }).run();

}
