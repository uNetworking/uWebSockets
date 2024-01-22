#include "App.h"
#include "LocalCluster.h"

int main() {
    /* Note that SSL is disabled unless you build with WITH_OPENSSL=1 */
    uWS::LocalCluster({
        .key_file_name = "misc/key.pem",
        .cert_file_name = "misc/cert.pem",
        .passphrase = "1234"
    },
    [](uWS::SSLApp &app) {
        /* Here this App instance is defined */
        app.get("/*", [](auto *res, auto * /*req*/) {
            res->end("Hello world!");
        }).listen(3000, [](auto *listen_socket) {
            if (listen_socket) {
                /* Note that us_listen_socket_t is castable to us_socket_t */
                std::cout << "Thread " << std::this_thread::get_id() << " listening on port " << us_socket_local_port(true, (struct us_socket_t *) listen_socket) << std::endl;
            } else {
                std::cout << "Thread " << std::this_thread::get_id() << " failed to listen on port 3000" << std::endl;
            }
        });
    });
}
