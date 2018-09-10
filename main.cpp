#include "App.h"

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

#include "new_design/HttpContext.h"
#include "new_design/HttpResponse.h"

int main(int argc, char **argv) {

    // new stuff, hope it sticks together?

    uWS::Loop loop;

    HttpContext<false> *httpContext = HttpContext<false>::create(loop.loop);

    // req, res?
    httpContext->onGet("/", [](auto *res) { // maybe use the terminology of HttpRequest for both?

        std::cout << "Why hello! How do we access the headers?" << std::endl;

        // read some data being passed
        res->read([](std::string_view chunk) {
            std::cout << "Reading some streamed in data:" << chunk << std::endl;
        });

        // res->writeHeader()->write();

        // req

    });

    httpContext->listen();

    httpContext->free();

    return 0;

    ////////////////////////////////////////////////////////////////////////////////


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

    struct UserData {

    };

    // serve a chat page with pub/sub and index.html and get (should be the main app of use)
    // basically take the old web chat page and upgrade it

    uWS::App a; // or uWS::SSLApp(options) for SSL

    a.onGet("/", [](auto *s, auto *req, auto *args) {

        std::cout << "URL: <" << req->getUrl() << ">" << std::endl;
        std::cout << "Query: <" << req->getQuery() << ">" << std::endl;
        std::cout << "User-Agent: <" << req->getHeader("user-agent") << ">" << std::endl;

    }).onWebSocket<UserData>("/ws", [](auto *ws, auto *req, auto *args) {

        std::cout << "WebSocket connected to /wsApi" << std::endl;

    }).onMessage<true>([](auto *ws, auto message/*, auto opCode*/) {

        std::cout << "WebSocket data: " << message << std::endl;

        //ws->send(message, opCode);

    }).onClose([](/*auto *ws, int code, auto message*/) {

        //std::cout << "WebSocket disconnected from /wsApi" << std::endl;

    }).listen("localhost", 3000, 0);

    /*auto serve = [](auto *s, auto *req, auto *args) {

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

        s->write([file](int offset) {
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
    }).listen(nullptr, 3000, 0);*/

    uWS::run();
    // loop.run();
}
