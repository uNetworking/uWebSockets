// this is roughly the interfaces I plan for (with changes and additional helpers added with time)
// much speaks for a header-only or header-mostly implementation now that uSockets is properly isolating its internal headers

#include "uWS.h"

int main() {

    std::cout << "HttpSocket size: " << sizeof(HttpSocket::Data) << std::endl;
    char *buffer = new char[512];

    // either use this, or use onHttpRoute but not both
    uWS::defaultHub.onHttpRequest([buffer](HttpSocket *s, HttpRequest *req) {

        if (req->getUrl() == "/") {
            s->writeStatus(200)->writeHeader("Server", "µWebSockets v0.15")->end(buffer, 512);
        } else {
            std::cout << "Got HTTP request at URL: " << req->getUrl() << std::endl;
        }

    }).listen("localhost", 3000, 0).run();

    // todo: important swapping to SSL should be .secureListen(same interfaces) and work out of the box!
}
