// this is roughly the interfaces I plan for (with changes and additional helpers added with time)
// much speaks for a header-only or header-mostly implementation now that uSockets is properly isolating its internal headers

#include "Context.h"
#include "HttpRouter.h"

int main() {

    std::cout << "HttpSocket size: " << sizeof(HttpSocket<true>::Data) << std::endl;
    char *buffer = new char[512];

    // SSL options are given via uSockets structure
    us_ssl_socket_context_options options = {};
    options.key_file_name = "/home/alexhultman/uWebSockets/misc/ssl/key.pem";
    options.cert_file_name = "/home/alexhultman/uWebSockets/misc/ssl/cert.pem";
    options.passphrase = "1234";

    //uWS::SSLContext c(options);
    uWS::Context c;

    c.route("GET", "/", [buffer](auto *s, HttpRequest *req, auto *args) {

        s->writeStatus(200)->writeHeader("Hello", "World")->end(buffer, 512);

    }).route("GET", "/wrong", [buffer](auto *s, HttpRequest *req, auto *args) {

        std::cout << "Wrong way!" << std::endl;

    }).listen("localhost", 3000, 0);

    uWS::defaultHub.run();
}
