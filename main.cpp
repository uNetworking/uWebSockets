#include "uWS.h"

std::string buffer;
int connections = 0;

template<int SIZE, class T>
void respond(T *s) {
    s->writeStatus("200 OK")->write([](int offset) {
        return std::string_view(buffer.data() + offset, SIZE - offset);
    }, SIZE);
}

//#define USE_SSL

int main(int argc, char **argv) {

    // 50 mb for huge
    buffer.resize(52428800);

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

    // todo: add timeouts and socket shutdown!

    app.onGet("/", [](auto *s, auto *req, auto *args) {
        s->writeStatus("200 OK")->writeHeader("Content-type", "text/html; charset=utf-8")->end("<h1>Welcome to µWebSockets v0.15!</h1>");
    }).onGet("/tiny", [](auto *s, auto *req, auto *args) {
        respond<512>(s);
    }).onGet("/small", [](auto *s, auto *req, auto *args) {
        respond<4096>(s);
    }).onGet("/medium", [](auto *s, auto *req, auto *args) {
        respond<16384>(s);
    }).onGet("/large", [](auto *s, auto *req, auto *args) {
        respond<51200>(s);
    }).onGet("/huge", [](auto *s, auto *req, auto *args) {
        respond<52428800>(s);
    }).onPost("/upload", [](auto *s, auto *req, auto *args) {

        s->read([s](std::string_view chunk) {
            std::cout << "Received chunk on URL /upload: <" << chunk << ">" << std::endl;

            s->writeStatus("200 OK")->end("Thanks for posting!");
        });

    }).onWebSocket("/wsApi", []() {

    }).onHttpConnection([](auto *s) {
        std::cout << "Connections: " << ++connections << std::endl;
    }).onHttpDisconnection([](auto *s) {
        std::cout << "Connections: " << --connections << std::endl;
    }).listen("localhost", 3000, 0);

    uWS::run();
    // loop.run();
}
