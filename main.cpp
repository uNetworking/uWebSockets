#include <string>

char largeBuf[] = "HTTP/1.1 200 OK\r\nContent-Length: 52428800\r\n\r\n";
int largeHttpBufSize = sizeof(largeBuf) + 52428800 - 1;
char *largeHttpBuf;

#include "uWS.h"

//#define USE_SSL

int main() {

    std::string buffer;
    buffer.resize(largeHttpBufSize);

    std::vector<char> vec;
    vec.resize(largeHttpBufSize);

    // all STD-classes's memory cut throughput in half!
    largeHttpBuf = (char *) buffer.data();//new char[largeHttpBufSize];//aligned_alloc(16, largeHttpBufSize);//std::aligned_alloc//vec.data();//malloc(largeHttpBufSize);
    memcpy(largeHttpBuf, largeBuf, sizeof(largeBuf) - 1);
    printf("%d\n", largeHttpBufSize);

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
            return std::string_view(largeHttpBuf + offset, largeHttpBufSize - offset);
        }, largeHttpBufSize);

    }).onWebSocket("/wsApi", []() {

    }).listen("localhost", 3000, 0);

    uWS::run();
    // loop.run();
}
