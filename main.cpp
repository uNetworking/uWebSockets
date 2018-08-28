#include "uWS.h"

#include <fstream>
#include <sstream>

std::string buffer;
int connections = 0;

template<int SIZE, class T>
void respond(T *s) {
    s->writeStatus("200 OK")->write([](int offset) {
        return std::string_view(buffer.data() + offset, SIZE - offset);
    }, SIZE);
}

#define USE_SSL

#include <map>

std::string_view getFile(std::string_view file) {
    static std::map<std::string_view, std::string_view> cache;

    auto it = cache.find(file);

    if (it == cache.end()) {
        std::cout << "Did not have file: " << file << std::endl;

        std::ifstream fin("rocket_files/" + std::string(file), std::ios::binary);
        std::ostringstream oss;
        oss << fin.rdbuf();

        char *cachedFile = (char *) malloc(oss.str().size());
        memcpy(cachedFile, oss.str().data(), oss.str().size());

        char *key = (char *) malloc(file.length());
        memcpy(key, file.data(), file.length());

        std::cout << "Size: " << oss.str().size() << std::endl;

        cache[std::string_view(key, file.length())] = std::string_view(cachedFile, oss.str().size());

        return getFile(file);
    } else {
        return it->second;
    }
}

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

    auto serve = [](auto *s, auto *req, auto *args) {

        //std::cout << "URL: " << req->getUrl() << std::endl;

        s->writeStatus("200 OK");
        std::string_view file;
        if (args->size() != 2) {
            file = getFile("rocket.html");
        } else {
            file = getFile((*args)[1]);

            std::string_view name = (*args)[1];

            if (name.length() > 4 && name.substr(name.length() - 4) == ".svg") {
                s->writeHeader("Content-type", "image/svg+xml");
            }
        }

                /*writeHeader("Content-type", "text/html; charset=utf-8")->*/s->write([file](int offset) {
            return std::string_view(file.data() + offset, file.size() - offset);
        }, file.size());

    };

    app.onGet("/", serve).onGet("/:folder/:file", serve).onGet("/tiny", [](auto *s, auto *req, auto *args) {
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
    }).listen(nullptr, 3000, 0);

    uWS::run();
    // loop.run();
}
