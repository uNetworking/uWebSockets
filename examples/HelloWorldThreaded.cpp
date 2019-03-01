#include "App.h"
#include <thread>
#include <algorithm>

int main() {
    /* Overly simple hello world app, using multiple threads */
    std::vector<std::thread *> threads(std::thread::hardware_concurrency());

    std::transform(threads.begin(), threads.end(), threads.begin(), [](std::thread *t) {
        return new std::thread([]() {

            uWS::App().get("/*", [](auto *res, auto *req) {
                res->end("Hello world!");
            }).listen(3000, [](auto *token) {
                if (token) {
                    std::cout << "Thread " << std::this_thread::get_id() << " listening on port " << 3000 << std::endl;
                } else {
                    std::cout << "Thread " << std::this_thread::get_id() << " failed to listen on port 3000" << std::endl;
                }
            }).run();

        });
    });

    std::for_each(threads.begin(), threads.end(), [](std::thread *t) {
        t->join();
    });
}
