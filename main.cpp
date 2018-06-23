#include "uWS.h"

std::string buffer;

//#define USE_SSL

int main() {

    buffer = "Why hello there!";

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

        // this one is simple and okay for small sends
        /*s->writeStatus("200 OK")
         ->writeHeader("Server", "uWebSockets")
         ->end(buffer);*/

        // for large sends you want a stream!
        s->writeStatus("200 OK")->write([](int offset) {
            return std::string_view(buffer.data() + offset, buffer.length() - offset);
        }, buffer.length());

    }).onWebSocket("/wsApi", []() {

    }).listen("localhost", 3000, 0);

    uWS::run();
    // loop.run();
}
