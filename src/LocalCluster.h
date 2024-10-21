/* This header is highly experimental and needs refactorings but will do for now */

#include <thread>
#include <algorithm>
#include <mutex>

unsigned int roundRobin = 0;
unsigned int hardwareConcurrency = std::thread::hardware_concurrency();
std::vector<std::thread *> threads(hardwareConcurrency);
std::vector<uWS::SSLApp *> apps;
std::mutex m;

namespace uWS {
struct LocalCluster {

    //std::vector<std::thread *> threads = std::thread::hardware_concurrency();
    //std::vector<uWS::SSLApp *> apps;
    //std::mutex m;


    static void loadBalancer() {
        static std::atomic<unsigned int> roundRobin = 0; // atomic fetch_add
    }

    LocalCluster(SocketContextOptions options = {}, std::function<void(uWS::SSLApp &)> cb = nullptr) {
        std::transform(threads.begin(), threads.end(), threads.begin(), [options, &cb](std::thread *) {

            return new std::thread([options, &cb]() {

                // lock this
                m.lock();
                apps.emplace_back(new uWS::SSLApp(options));
                uWS::SSLApp *app = apps.back();

                cb(*app);
                
                app->preOpen([](struct us_socket_context_t *context, LIBUS_SOCKET_DESCRIPTOR fd) -> LIBUS_SOCKET_DESCRIPTOR {

                    std::ignore = context;

                    /* Distribute this socket in round robin fashion */
                    //std::cout << "About to load balance " << fd << " to " << roundRobin << std::endl;

                    auto receivingApp = apps[roundRobin];
                    apps[roundRobin]->getLoop()->defer([fd, receivingApp]() {
                        receivingApp->adoptSocket(fd);
                    });

                    roundRobin = (roundRobin + 1) % hardwareConcurrency;
                    return (LIBUS_SOCKET_DESCRIPTOR) -1;
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
};
}