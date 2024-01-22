#include "App.h"
#include <thread>
#include <algorithm>
#include <mutex>

/* Note that SSL is disabled unless you build with WITH_OPENSSL=1 */
const int SSL = 1;

unsigned int roundRobin = 0;
unsigned int hardwareConcurrency = std::thread::hardware_concurrency();
std::vector<std::thread *> threads(hardwareConcurrency);
std::vector<uWS::SSLApp *> apps;
std::mutex m;

namespace uWS {
struct LocalCluster {

    //std::vector<std::thread *> threads = std::thread::hardware_concurrency();
    std::vector<uWS::SSLApp *> apps;
    std::mutex m;


    static void loadBalancer() {
        static std::atomic<unsigned int> roundRobin = 0; // atomic fetch_add
    }

    LocalCluster(SocketContextOptions options = {}, std::function<void(uWS::SSLApp &)> cb = nullptr) {

    }
};
}

int main() {

    // can be strictly round robin or not

    // uWS::LocalCluster({
    //     .key_file_name = "misc/key.pem",
    //     .cert_file_name = "misc/cert.pem",
    //     .passphrase = "1234"
    // },
    // [](uWS::SSLApp &app) {
    //     /* Here this App instance is defined */
    //     app.get("/*", [](auto *res, auto * /*req*/) {
    //         res->end("Hello world!");
    //     }).listen(3000, [](auto *listen_socket) {
    //         if (listen_socket) {
    //             /* Note that us_listen_socket_t is castable to us_socket_t */
    //             std::cout << "Thread " << std::this_thread::get_id() << " listening on port " << us_socket_local_port(SSL, (struct us_socket_t *) listen_socket) << std::endl;
    //         } else {
    //             std::cout << "Thread " << std::this_thread::get_id() << " failed to listen on port 3000" << std::endl;
    //         }
    //     });
    // });

    std::transform(threads.begin(), threads.end(), threads.begin(), [](std::thread *) {

        return new std::thread([]() {

            // lock this
            m.lock();
            apps.emplace_back(new uWS::SSLApp({
                .key_file_name = "misc/key.pem",
                .cert_file_name = "misc/cert.pem",
                .passphrase = "1234"
            }));
            uWS::SSLApp *app = apps.back();

            app->get("/*", [](auto *res, auto * /*req*/) {
                res->end("Hello world!");
            }).listen(3000, [](auto *listen_socket) {
                if (listen_socket) {
                    /* Note that us_listen_socket_t is castable to us_socket_t */
                    std::cout << "Thread " << std::this_thread::get_id() << " listening on port " << us_socket_local_port(SSL, (struct us_socket_t *) listen_socket) << std::endl;
                } else {
                    std::cout << "Thread " << std::this_thread::get_id() << " failed to listen on port 3000" << std::endl;
                }
            }).preOpen([](LIBUS_SOCKET_DESCRIPTOR fd) {

                /* Distribute this socket in round robin fashion */
                std::cout << "About to load balance " << fd << " to " << roundRobin << std::endl;

                auto receivingApp = apps[roundRobin];
                apps[roundRobin]->getLoop()->defer([fd, receivingApp]() {
                    receivingApp->adoptSocket(fd);
                });

                roundRobin = (roundRobin + 1) % hardwareConcurrency;
                return -1;
            });
            m.unlock();
            app->run();
            std::cout << "Fallthrough!" << std::endl;
            delete app;
        });
    });

    std::for_each(threads.begin(), threads.end(), [](std::thread *t) {
        t->join();
    });
}
