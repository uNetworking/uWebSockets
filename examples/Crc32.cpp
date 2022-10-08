#include "App.h"

/* This is a good example for testing and showing the POST requests.
 * Anything you post (either with content-length or using transfer-encoding: chunked) will
 * be hashed with crc32 and sent back in the response. This example also shows how to deal with
 * aborted requests. */

/* Note that uWS::SSLApp({options}) is the same as uWS::App() when compiled without SSL support */

#include <zlib.h>
#include <sstream>

int main() {

	uWS::SSLApp({
	  .key_file_name = "misc/key.pem",
	  .cert_file_name = "misc/cert.pem",
	  .passphrase = "1234"
	}).post("/*", [](auto *res, auto *req) {

		/* Display the headers */
		std::cout << " --- " << req->getUrl() << " --- " << std::endl;
		for (auto [key, value] : *req) {
			std::cout << key << ": " << value << std::endl;
		}

		auto isAborted = std::make_shared<bool>(false);
		uLong crc = crc32(0L, Z_NULL, 0);
		res->onData([res, isAborted, crc](std::string_view chunk, bool isFin) mutable {
			if (chunk.length()) {
				crc = crc32(crc, (const Bytef *) chunk.data(), (uInt) chunk.length());
			}

			if (isFin && !*isAborted) {
				std::stringstream s;
    			s << std::hex << crc;
				res->end(s.str());
			}
		});

		res->onAborted([isAborted]() {
			*isAborted = true;
		});
	}).listen(3000, [](auto *listen_socket) {
	    if (listen_socket) {
			std::cerr << "Listening on port " << 3000 << std::endl;
	    }
	}).run();

	std::cout << "Failed to listen on port 3000" << std::endl;
}
