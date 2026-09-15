#include "App.h"
#include <iostream>

/* Passing two integers as last arguments (lower and upper expiry) makes the route cached */
#define LOWER_EXPIRY 1
#define UPPER_EXPIRY 5

int main() {
    uWS::App app;

    /* Regular, non-cached response */
    app.get("/not-cached", [](auto *res, auto */*req*/) {
        res->end("Responding without a cache");
    }).get("/*", [](auto *res, auto */*req*/) {
        /* A cached response with 5 seconds of lifetime */
        std::cout << "Filling cache now" << std::endl;
        res->end("This is a response");
    }, LOWER_EXPIRY, UPPER_EXPIRY).listen(8080, [](bool success) {
        if (success) {
            std::cout << "Listening on port 8080" << std::endl;
        } else {
            std::cerr << "Failed to listen on port 8080" << std::endl;
        }
    });

    app.run();
}
