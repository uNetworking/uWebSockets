#include <string>

std::string buffer;

#include "uWS.h"

//#define USE_SSL

int main() {

    buffer = "HTTP/1.1 200 OK\r\nContent-Length: 52428800\r\n\r\n";
    buffer.resize(52428800 + buffer.length());

    //uWS::init();
    //uWS::Loop loop();

#ifdef USE_SSL
    uWS::SSLApp app(uWS::SSLOptions()
                  .keyFileName("/home/alexhultman/uWebSockets/misc/ssl/key.pem")
                  .certFileName("/home/alexhultman/uWebSockets/misc/ssl/cert.pem")
                  .passphrase("1234"));
#else
    uWS::App app;
#endif

    app.onGet("/", [](auto *s, auto *req, auto *args) {

        s->/*writeStatus("200 OK")->*/write([](int offset) {
            return std::string_view(buffer.data() + offset, buffer.length() - offset);
        }, buffer.length());

    }).onWebSocket("/wsApi", []() {

    }).listen("localhost", 3000, 0);

    uWS::run();
    // loop.run();
}
