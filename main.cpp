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

int main(int argc, char **argv) {

    // this part is a bit too C-ish?
    us_ssl_socket_context_options sslOptions;
    sslOptions.key_file_name = "/home/alexhultman/uWebSockets/misc/ssl/key.pem";
    sslOptions.cert_file_name = "/home/alexhultman/uWebSockets/misc/ssl/cert.pem";
    sslOptions.passphrase = "1234";

    uWS::SSLApp(sslOptions).get("/hello", [](auto *res, auto *req) {
        res->writeStatus(uWS::HTTP_200_OK)->write("Hello world!");
    }).get("/:folder/:file", [](auto *res, auto *req) {
        res->writeStatus(uWS::HTTP_200_OK)->write(getFile((req->getUrl() == "/" ? "/rocket_files/rocket.html" : req->getUrl()).substr(1)));
    }).listen(3000, [](auto *token) {
        if (token) {
            std::cout << "Listening on port " << 3000 << std::endl;
        }
    }).run();

}
