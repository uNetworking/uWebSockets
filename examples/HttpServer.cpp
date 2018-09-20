/* This is a static HTTP(S) server. It caches the files in given root folder and streams them by request */

#include <App.h>

/* Helpers for this example */
#include "helpers/FileCache.h"
#include "helpers/Middleware.h"

int main(int argc, char **argv) {

    if (argc < 3 || argc > 7) {
        std::cout << "Usage: HttpServer root port [ssl_cert ssl_key ssl_dh_params ssl_passphrase]" << std::endl;
        return 0;
    }

    int port = atoi(argv[2]);
    const char *root = argv[1];
    /* Cache files of root folder */
    FileCache fileCache(root);

    /* Either serve over HTTP or HTTPS */
    if (argc > 5) {
        /* HTTPS */
        uWS::SSLApp({
            .cert_file_name = argv[3], /* Required */
            .key_file_name = argv[4], /* Required */
            .dh_params_file_name = argv[5], /* Required (in this example) */
            .passphrase = (argc == 7 ? argv[6] : nullptr) /* Passphrase is optional */
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
