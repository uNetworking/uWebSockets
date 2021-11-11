#include "App.h"

/* Note that uWS::SSLApp({options}) is the same as uWS::App() when compiled without SSL support */

struct us_listen_socket_t *globalListenSocket;

int main() {
	/* Overly simple hello world app (SNI) */
	uWS::SSLApp app = uWS::SSLApp({
	  .key_file_name = "../misc/key.pem",
	  .cert_file_name = "../misc/cert.pem",
	  .passphrase = "1234"
	}).missingServerName([&app](const char *hostname) {

		printf("We are missing server name: <%s>\n", hostname);

		/* Assume it is localhost, so add it */
		app.addServerName("localhost", {
			.key_file_name = "../misc/key.pem",
			.cert_file_name = "../misc/cert.pem",
			.passphrase = "1234"
		});

	}).get("/*", [](auto *res, auto */*req*/) {
	    res->end("Hello world!");
	}).get("/exit", [](auto *res, auto */*req*/) {
             res->end("Shutting down!");
            /* We use this to check graceful closedown */
            us_listen_socket_close(1, globalListenSocket);
	}).listen(3000, [](auto *listenSocket) {
	    if (listenSocket) {
			std::cout << "Listening on port " << 3000 << std::endl;
			globalListenSocket = listenSocket;
	    } else {
			std::cout << "Failed to listen on port 3000" << std::endl;
		}
	});

	/* Let's add a wildcard SNI to begin with */
	app.addServerName("*.google.*", {
		.key_file_name = "../misc/key.pem",
		.cert_file_name = "../misc/cert.pem",
		.passphrase = "1234"
	});

	app.run();
}
