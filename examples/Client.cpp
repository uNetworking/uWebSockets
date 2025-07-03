// This example is broken and doesn't do anything. It is a potential interface for a future potential client.
// There is no client support implemented in the library. This example is a placeholder.

#include "ClientApp.h"
#include <iostream>

int main() {
    uWS::ClientApp app(uWS::WebSocketClientBehavior{
        .open = [](/*auto *ws*/) {
            std::cout << "Hello and welcome to client" << std::endl;
        },
        .message = [](std::string_view) {

        },
        .close = [](/*auto *ws*/) {
            std::cout << "bye" << std::endl;
        }
    });
    
    app.connect("ws://localhost:3000", "protocol");
    
    app.run();
}
