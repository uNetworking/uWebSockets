#include "App.h"
#include <iostream>

int main() {
    uWS::App app;

    /* Regular, non-cached response */
    app.get("/not-cached", [](auto *res, auto */*req*/) {
        res->end("Responding without a cache");
    }).get("/*", [](auto *res, auto */*req*/) {
        /* A cached response with 5 seconds of lifetime */
        std::cout << "Filling cache now" << std::endl;
        res->end("This is a response");
    }, 5).listen(8080, [](bool success) {
        if (success) {
            std::cout << "Listening on port 8080" << std::endl;
        } else {
            std::cerr << "Failed to listen on port 8080" << std::endl;
        }
    });

    app.run();
}
