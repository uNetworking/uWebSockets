/* This is a simple HTTP(S) web server much like Python's SimpleHTTPServer */

#include <App.h>

/* Helpers for this example */
#include "helpers/AsyncFileReader.h"
#include "helpers/Middleware.h"

/* optparse */
#define OPTPARSE_IMPLEMENTATION
#include "helpers/optparse.h"

int main(int argc, char **argv) {

    int option;
    struct optparse options;
    optparse_init(&options, argv);

    struct optparse_long longopts[] = {
        {"port", 'p', OPTPARSE_REQUIRED},
        {"help", 'h', OPTPARSE_NONE},
        {"color", 'c', OPTPARSE_REQUIRED},
        {"delay", 'd', OPTPARSE_OPTIONAL},
        {0}
    };

    int port = 3000;

    while ((option = optparse_long(&options, longopts, nullptr)) != -1) {
        switch (option) {
        case 'p':
            port = /*options.optarg ? */atoi(options.optarg);// : port;
            break;
        case 'h':

            //break;
        /*case 'b':
            brief = true;
            break;
        case 'c':
            color = options.optarg;
            break;
        case 'd':
            delay = options.optarg ? atoi(options.optarg) : 1;
            break;*/
        case '?':
            std::cout << "Usage: " << argv[0] << " [--help] [--port <port>] [--ssl <cert> <key>] [--passphrase <ssl key passphrase>] [--dh_params <ssl dh params file>] <public root>" << std::endl;

            return 0;
            //fprintf(stderr, "%s: %s\n", argv[0], options.errmsg);
            //exit(EXIT_FAILURE);
        }
    }

    /* Print remaining arguments. */
    char *arg;
       while ((arg = optparse_arg(&options)))
           printf("%s\n", arg);

    std::cout << "Port is " << port << std::endl;

    return 0;

    if (argc < 3 || argc > 7) {
        std::cout << "Usage: HttpServer root port [ssl_cert ssl_key ssl_dh_params ssl_passphrase]" << std::endl;
        return 0;
    }

    //int port = atoi(argv[2]);
    const char *root = argv[1];
    /* Cache files of root folder */
    //FileCache fileCache(root);

    /* Either serve over HTTP or HTTPS */
    if (argc > 5) {
        /* HTTPS */
        uWS::SSLApp({
            .cert_file_name = argv[3], /* Required */
            .key_file_name = argv[4], /* Required */
            .dh_params_file_name = argv[5], /* Required (in this example) */
            .passphrase = (argc == 7 ? argv[6] : nullptr) /* Passphrase is optional */
        }).get("/*", [](auto *res, auto *req) {
            //serveFile(res, req)->write(fileCache.getFile(req->getUrl()));
        }).listen(port, [port, root](auto *token) {
            if (token) {
                std::cout << "Serving " << root << " over HTTPS a " << port << std::endl;
            }
        }).run();
    } else {
        /* HTTP */
        uWS::App().get("/*", [](auto *res, auto *req) {
            //serveFile(res, req)->write(fileCache.getFile(req->getUrl()));
        }).listen(port, [port, root](auto *token) {
            if (token) {
                std::cout << "Serving " << root << " over HTTP a " << port << std::endl;
            }
        }).run();
    }

    std::cout << "Failed to listen to port " << port << std::endl;
}
