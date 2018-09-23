#include "App.h"

#include <map>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>

// should probably fix this one up a bit some time
std::string_view getFile(std::string_view file) {
    static std::map<std::string_view, std::string_view> cache;

    auto it = cache.find(file);

    if (it == cache.end()) {
        std::cout << "Did not have file: " << file << std::endl;

        std::ifstream fin(std::string(file), std::ios::binary);
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

#include <set>

std::set<uWS::HttpResponse<false> *> delayedResponses;

int main(int argc, char **argv) {

    // create a timer that resumes sockets
    auto *timer = us_create_timer((us_loop *) uWS::Loop::defaultLoop(), 1, 0);
    us_timer_set(timer, [](auto *timer) {

        for (auto *x : delayedResponses) {
            std::cout << "Resuming a response now!" << std::endl;
            x->resume();
        }

        delayedResponses.clear();

    }, 1000, 1000);

    uWS::/*SSL*/App(/*{
        .key_file_name = "/home/alexhultman/uWebSockets/misc/ssl/key.pem",
        .cert_file_name = "/home/alexhultman/uWebSockets/misc/ssl/cert.pem",
        .dh_params_file_name = "/home/alexhultman/dhparams.pem",
        .passphrase = "1234"
    }*/).get("/", [](auto *res, auto *req) {
        res->writeStatus(uWS::HTTP_200_OK)->write("Hello world!");
    }).get("/endless", [](auto *res, auto *req) {
        /* This route doesn't specify any length and only returns 1 char per chunk */
        res->write([](int offset) {
            std::string_view data("<html><body><h1>Hello, world</h1></body></html>");
            if (offset < data.length()) {
                return std::make_pair<bool, std::string_view>(offset < data.length() - 1, data.substr(offset, 1));
            }
            return uWS::HTTP_STREAM_FIN;
        });
    }).get("/delayed", [](auto *res, auto *req) {
        /* This route streams back chunks of data in delayed fashion */
        res->writeStatus(uWS::HTTP_200_OK)->write([res](int offset) {

            /* Handle aborted stream out */
            if (offset == -1) {
                std::cout << "Removing closed stream!" << std::endl;
                delayedResponses.erase(res);
                return uWS::HTTP_STREAM_FIN;
            }

            /* Delay stream out */
            std::cout << "Delaying stream now" << std::endl;
            delayedResponses.insert(res);
            return uWS::HTTP_STREAM_PAUSE;

        }, 100);
    }).get("/:folder/:file", [](auto *res, auto *req) {
        res->writeStatus(uWS::HTTP_200_OK)->write(getFile((req->getUrl() == "/" ? "/rocket_files/rocket.html" : req->getUrl()).substr(1)));
    }).listen(3000, [](auto *token) {
        if (token) {
            std::cout << "Listening on port " << 3000 << std::endl;
        }
    }).run();

}
