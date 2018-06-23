// this is roughly the interfaces I plan for (with changes and additional helpers added with time)
// much speaks for a header-only or header-mostly implementation now that uSockets is properly isolating its internal headers

#include "App.h"
#include "HttpRouter.h"

int main() {

    std::cout << "HttpSocket size: " << sizeof(HttpSocket<true>::Data) << std::endl;
    char *buffer = new char[512];

    uWS::SSLApp c(uWS::SSLOptions()
                  .keyFileName("/home/alexhultman/uWebSockets/misc/ssl/key.pem")
                  .certFileName("/home/alexhultman/uWebSockets/misc/ssl/cert.pem")
                  .passphrase("1234"));
    //uWS::App c;

    c.onGet("/", [buffer](auto *s, HttpRequest *req, auto *args) {

        s->writeStatus(200)->writeHeader("Hello", "World")->end(buffer, 512);

    }).onGet("/wrong", [buffer](auto *s, HttpRequest *req, auto *args) {

        std::cout << "Wrong way!" << std::endl;

    }).onWebSocket([]() {

    }).listen("localhost", 3000, 0);

    uWS::run();
}
