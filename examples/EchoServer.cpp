#include "App.h"

int main() {
    /* Very simple WebSocket echo server */
    uWS::App().ws<void>("/*", {
        /*.compression = */true,
        /*.maxPayloadLength*/

        /*.open = */[](auto *ws, auto *req) {

        },
        /*.message = */[](auto *ws, std::string_view message, uWS::OpCode opCode) {
            ws->send(message, opCode);
        }
        /*.drain = []() {

        },
        .ping = []() {

        },
        .pong = []() {

        },
        .close = []() {

        }*/
    }).listen(9001, [](auto *token) {
        if (token) {
            std::cout << "Listening on port " << 9001 << std::endl;
        }
    }).run();
}
