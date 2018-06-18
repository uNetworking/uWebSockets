// uWS.h
#include "Hub.h"

char largeBuf[] = "HTTP/1.1 200 OK\r\nContent-Length: 512\r\n\r\n";
int largeHttpBufSize = sizeof(largeBuf) + 512 - 1;
char *largeHttpBuf;

int main() {
    largeHttpBuf = (char *) malloc(largeHttpBufSize);
    memcpy(largeHttpBuf, largeBuf, sizeof(largeBuf) - 1);

    std::cout << "HttpSocket size: " << sizeof(HttpSocket) << std::endl;

    // either use this, or use onHttpRoute but not both
    uWS::defaultHub.onHttpRequest([](HttpSocket *s, HttpRequest *req) {

        // we do not care for routers just yet!
        if (req->getUrl() == "/") {
            // just write back for now!

            us_socket *us = (us_socket *) s;
            us_socket_write(us, largeHttpBuf, largeHttpBufSize, 0);

        } else {


            std::cout << "Got HTTP request at URL: " << req->getUrl() << std::endl;
        }


        // get the URL here!



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
