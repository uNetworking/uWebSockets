// uWS.h
#include "Hub.h"
#include <iostream>
#include <string.h>

//char largeBuf[] = "HTTP/1.1 200 OK\r\nContent-Length: 512\r\n\r\n";
//int largeHttpBufSize = sizeof(largeBuf) + 512 - 1;

char largeBuf[] = "HTTP/1.1 200 OK\r\nContent-Length: 52428800\r\n\r\n";
int largeHttpBufSize = sizeof(largeBuf) + 52428800 - 1;

char *largeHttpBuf;

int main() {
    std::cout << "HttpSocket size: " << sizeof(HttpSocket) << std::endl;

    largeHttpBuf = (char *) malloc(largeHttpBufSize);
    memcpy(largeHttpBuf, largeBuf, sizeof(largeBuf) - 1);

    Hub h;

    h.onHttpConnection([](HttpSocket *s) {
        //std::cout << "HTTP connection" << std::endl;
    });

    // req = ?, res = HttpSocket
    h.onHttpRequest([](HttpSocket *s, auto req, char *data, unsigned int length) {

        //std::cout << "Got data: " << std::string(data, length) << std::endl;

        // s->writeStatus();
        // s->writeHeader();

        // if url == / then stream index.htm

        s->stream(largeHttpBufSize, [](int offset) {
            return std::make_pair(largeHttpBuf + offset, largeHttpBufSize - offset);
        });
    });

    h.onHttpDisconnection([](HttpSocket *s) {
        //std::cout << "HTTP disconnection" << std::endl;
    });

    h.listen(nullptr, 3000, 0);
    h.run();
}
