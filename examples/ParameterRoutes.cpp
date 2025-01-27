#include "App.h"

/* Note that uWS::SSLApp({options}) is the same as uWS::App() when compiled without SSL support */

int main() {
    /* Overly simple hello world app */
    uWS::SSLApp({
      .key_file_name = "misc/key.pem",
      .cert_file_name = "misc/cert.pem",
      .passphrase = "1234"
    }).get("/:first/static/:second", [](auto *res, auto *req) {

        res->write("<h1>first is: ");
        res->write(req->getParameter("first"));
        res->write("</h1>");

        res->write("<h1>second is: ");
        res->write(req->getParameter("second"));
        res->end("</h1>");

    }).listen(3000, [](auto *listen_socket) {
        if (listen_socket) {
            std::cout << "Listening on port " << 3000 << std::endl;
        }
    }).run();

    std::cout << "Failed to listen on port 3000" << std::endl;
}
