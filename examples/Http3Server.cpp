#ifdef LIBUS_USE_QUIC

/* Do not rely on this API, it will change */
#include "Http3App.h"
#include <iostream>

/* Tentative example of simple Http3 server */
int main() {
	uWS::H3App({
	  .key_file_name = "../misc/key.pem",
	  .cert_file_name = "../misc/cert.pem",
	  .passphrase = "1234"
	}).get("/*", [](auto *res, auto *req) {

		/* Printing these should obviously be disabled if doing benchmarking */
		//std::cout << req->getHeader(":path") << std::endl;
		//std::cout << req->getHeader(":method") << std::endl;

	    res->end("Hello H3 from uWS!");
	}).listen(9004, [](auto *listen_socket) {
	    if (listen_socket) {
			std::cout << "Listening on port " << 9004 << std::endl;
	    }
	}).run();

	std::cout << "Failed to listen on port 3000" << std::endl;
}

#else

#include <stdio.h>

int main() {
    printf("Compile with WITH_QUIC=1 WITH_BORINGSSL=1 make in order to build this example\n");
}

#endif