/* Do not rely on this API, it will change */
#include "Http3App.h"
#include <iostream>

/* Tentative example of simple Http3 server */
int main() {
	uWS::QuicApp({
	  .key_file_name = "../misc/key.pem",
	  .cert_file_name = "../misc/cert.pem",
	  .passphrase = "1234"
	}).get("/*", [](auto *res, auto */*req*/) {
	    res->end("Hello quic!");
	}).listen(3000, [](auto *listen_socket) {
	    if (listen_socket) {
			std::cout << "Listening on port " << 3000 << std::endl;
	    }
	}).run();

	std::cout << "Failed to listen on port 3000" << std::endl;
}
