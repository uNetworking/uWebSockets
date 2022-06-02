#include "App.h"
#include <thread>
#include <algorithm>
#include <mutex>
#include <chrono>

/* Note that SSL is disabled unless you build with WITH_OPENSSL=1 */
const int SSL = 1;
std::mutex stdoutMutex;

int main() {
    /* Overly simple hello world app, using multiple threads */
    // std::vector<std::thread *> threads(std::thread::hardware_concurrency());

    // std::transform(threads.begin(), threads.end(), threads.begin(), [](std::thread */*t*/) {
    //     return new std::thread([]() {

            uWS::SSLApp({
                .key_file_name = "../misc/key.pem",
                .cert_file_name = "../misc/cert.pem",
                .passphrase = "1234"
            }).get("/*", [](auto *res, auto * req) {
                 struct UpgradeData {
                    std::string secWebSocketKey;
                    std::string secWebSocketProtocol;
                    std::string secWebSocketExtensions;
                    decltype(res) httpRes;
                    bool aborted = false;
                    std::mutex mutex;
                } *upgradeData = new UpgradeData {
                    std::string(req->getHeader("sec-websocket-key")),
                    std::string(req->getHeader("sec-websocket-protocol")),
                    std::string(req->getHeader("sec-websocket-extensions")),
                    res
                };
                 res->onAborted([=]() {
                    upgradeData->mutex.lock();
                    /* We don't implement any kind of cancellation here,
                    * so simply flag us as aborted */
                    upgradeData->aborted = true;
                    std::cout << "HTTP socket was closed before we upgraded it!" << std::endl;
                    upgradeData->mutex.unlock();
                });

                 new std::thread([=]() {
                    upgradeData->mutex.lock();
                        std::this_thread::sleep_for(std::chrono::microseconds(100));
                        if(!upgradeData->aborted)
                            upgradeData->httpRes->end("Hello world!");
                    upgradeData->mutex.unlock();
                    delete upgradeData;
                    
                 });

                // res->end("Hello world!");
            }).listen(3000, [](auto *listen_socket) {
		stdoutMutex.lock();
                if (listen_socket) {
                    /* Note that us_listen_socket_t is castable to us_socket_t */
                    std::cout << "Thread " << std::this_thread::get_id() << " listening on port " << us_socket_local_port(SSL, (struct us_socket_t *) listen_socket) << std::endl;
                } else {
                    std::cout << "Thread " << std::this_thread::get_id() << " failed to listen on port 3000" << std::endl;
                }
		stdoutMutex.unlock();
            }).run();

    //     });
    // });

    // std::for_each(threads.begin(), threads.end(), [](std::thread *t) {
    //     t->join();
    // });
}
