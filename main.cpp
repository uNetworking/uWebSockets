// uWS.h
#include "Hub.h"

int main() {
    std::cout << "HttpSocket size: " << sizeof(HttpSocket) << std::endl;

    // either use this, or use onHttpRoute but not both
    uWS::defaultHub.onHttpRequest([](HttpSocket *s, HttpRequest *req) {

        // an http socket can BOTH send buffered data AND have one stream in and one stream out!

        // writeStatus(234314234)
        // writeHeader(key, value)
        // streamOut(asdasdasdsad)

        // if some asshole posted data, stream it in
        /*s->streamIn([]() {

        });*/

        //std::cout << "Got data: " << std::string(data, length) << std::endl;

        // s->writeStatus();
        // s->writeHeader();

        // if url == / then stream index.htm

        // stream out the response
        /*s->stream(largeHttpBufSize, [](int offset) {
            return std::make_pair(largeHttpBuf + offset, largeHttpBufSize - offset);
        });*/
    }).listen("localhost", 3000, 0).run();

    // todo: important swapping to SSL should be .secureListen(same interfaces) and work out of the box!
}
