/* This is a static HTTP(S) server. It caches the files in given root folder and streams them by request */

#include <App.h>

/* Helpers for this example */
#include "FileCache.h"
#include "Middleware.h"

int main(int argc, char **argv) {

    if (argc < 3 || argc > 6) {
        std::cout << "Usage: static_http_server root port [ssl_cert ssl_key ssl_passphrase]" << std::endl;
        return 0;
    }

    int port = atoi(argv[2]);
    const char *root = argv[1];
    /* Cache files of root folder */
    FileCache fileCache(root);

    /* Either serve over HTTP or HTTPS */
    if (argc > 4) {
        /* HTTPS */
        uWS::SSLApp({
            .cert_file_name = argv[3],
            .key_file_name = argv[4],
            .passphrase = (argc == 6 ? argv[5] : nullptr)
        }).get("/*", [&fileCache](auto *res, auto *req) {
            serveFile(res, req)->write(fileCache.getFile(req->getUrl()));
        }).listen(port, [port, root](auto *token) {
            if (token) {
                std::cout << "Serving " << root << " over HTTPS a " << port << std::endl;
            }
        }).run();
    } else {
        /* HTTP */
        uWS::App().get("/*", [&fileCache](auto *res, auto *req) {
            serveFile(res, req)->write(fileCache.getFile(req->getUrl()));
        }).listen(port, [port, root](auto *token) {
            if (token) {
                std::cout << "Serving " << root << " over HTTP a " << port << std::endl;
            }
        }).run();
    }

    std::cout << "Failed to listen to port " << port << std::endl;
}
