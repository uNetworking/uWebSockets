#include <uWS/uWS.h>
#include <thread>
#include <algorithm>
#include <iostream>

int main() {
    std::vector<std::thread *> threads(std::thread::hardware_concurrency());
    std::transform(threads.begin(), threads.end(), threads.begin(), [](std::thread *t) {
        return new std::thread([]() {
            uWS::Hub h;

            h.onMessage([](uWS::WebSocket<uWS::SERVER> *ws, char *message, size_t length, uWS::OpCode opCode) {
                ws->send(message, length, opCode);
            });

            // This makes use of the SO_REUSEPORT of the Linux kernel
            // Other solutions include listening to one port per thread
            // with or without some kind of proxy inbetween
            if (!h.listen(3000, nullptr, uS::ListenOptions::REUSE_PORT)) {
                std::cout << "Failed to listen" << std::endl;
            }
            h.run();
        });
    });

    std::for_each(threads.begin(), threads.end(), [](std::thread *t) {
        t->join();
    });
}
