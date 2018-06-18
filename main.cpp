// uWS.h
#include "Hub.h"
#include <iostream>
#include <string.h>

int main() {
    std::cout << "HttpSocket size: " << sizeof(HttpSocket) << std::endl;

    // defaultHub is per thread ready for action
    // uWS::defaultHub().onHttpRequest().listen().isListening().run();

    Hub h;

    h.onHttpConnection([](HttpSocket *s) {
        std::cout << "HTTP connection" << std::endl;
    });

    // req = ?, res = HttpSocket
    // should be s, req, nothing more or less!
    h.onHttpRequest([](HttpSocket *s, auto req, char *data, unsigned int length) {

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
    });

    h.onHttpDisconnection([](HttpSocket *s) {
        std::cout << "HTTP disconnection" << std::endl;
    });

    h.listen(nullptr, 3000, 0);
    h.run();
}
