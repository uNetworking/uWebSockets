#ifdef LIBUS_USE_QUIC

/* Do not rely on this API, it will change */
#include "Http3App.h"
#include <iostream>

/* Example of simple Http3 server handling both GET with headers and POST with body.
 * You might be surprised to find out you can replace uWS::H3App with uWS::SSLApp and
 * serve TCP/TLS-based HTTP instead of QUIC-based HTTP, using the same very code ;) */
int main() {
	uWS::H3App({
	  .key_file_name = "misc/key.pem",
	  .cert_file_name = "misc/cert.pem",
	  .passphrase = "1234"
	}).get("/*", [](auto *res, auto *req) {

		/* Printing these should obviously be disabled if doing benchmarking */
		std::cout << req->getHeader(":path") << std::endl;
		std::cout << req->getHeader(":method") << std::endl;

	    res->end("Hello H3 from uWS!");
	}).post("/*", [](auto *res, auto */*req*/) {

		/* You also need to set onAborted if receiving data */
		res->onData([res, bodyBuffer = (std::string *)nullptr](std::string_view chunk, bool isLast) mutable {
			if (isLast) {
				if (bodyBuffer) {
					/* Send back the (chunked) body we got, as response */
					bodyBuffer->append(chunk);
					res->end(*bodyBuffer);
					delete bodyBuffer;
				} else {
					/* Send back the body we got, as response (fast path) */
					res->end(chunk);
				}
			} else {
				/* Slow path */
				if (!bodyBuffer) {
					bodyBuffer = new std::string;
				}
				/* If we got the body in a chunk, buffer it up until whole */
				bodyBuffer->append(chunk);
			}

		});

		/* If you have pending, asynch work, you should abort such work in this callback */
		res->onAborted([]() {
			/* Again, just printing is not enough, you need to abort any pending work here
			 * so that nothing will call res->end, since the request was aborted and deleted */
			printf("Stream was aborted!\n");
		});
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