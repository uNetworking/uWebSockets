#include "Client.h"
#include <iostream>

int main() {
    uWS::Client({
    .open = [](/*auto *ws*/) {
        std::cout << "Hello and welcome to client" << std::endl;
    },
    .message = [](/*auto *ws, auto message*/) {

    },
    .close = [](/*auto *ws*/) {
        std::cout << "bye" << std::endl;
    }
    }).connect("ws://localhost:3000").run();
}