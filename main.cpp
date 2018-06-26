#include "uWS.h"

std::string buffer;

//#define USE_SSL

int main(int argc, char **argv) {

    // dynamically set respinse size from arguments
    if (argc == 2) {
        int bytes = atoi(argv[1]);
        std::cout << "Server going to respond with " << bytes << " bytes of data" << std::endl;
        buffer.resize(bytes);
    } else {
        std::cout << "Usage: uWS_test bytesInResponse" << std::endl;
        return -1;
    }

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

        // streams need to expose more information about lifetime!
        s->writeStatus("200 OK")->write([](int offset) {
            return std::string_view(buffer.data() + offset, buffer.length() - offset);
        }, buffer.length());

    }).onPost("/upload", [](auto *s, auto *req, auto *args) {

        s->read([s](std::string_view chunk) {
            std::cout << "Received chunk on URL /upload: <" << chunk << ">" << std::endl;

            s->writeStatus("200 OK")->end("Thanks for posting!");
        });

    }).onWebSocket("/wsApi", []() {

    }).listen("localhost", 3000, 0);

    uWS::run();
    // loop.run();
}
