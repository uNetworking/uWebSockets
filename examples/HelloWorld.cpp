#include "App.h"

#include <thread>
#include <algorithm>
#include <mutex>
#include <ctime>

/* Note that uWS::SSLApp({options}) is the same as uWS::App() when compiled without SSL support */
std::mutex current_date_mutex;
char current_date_buffer[sizeof "2011-10-08T07:07:09Z"];

int main() {

	auto time_thread = new std::thread([]() {
		while(true){
			time_t now;	
    		time(&now);
			current_date_mutex.lock();
			strftime(current_date_buffer, sizeof current_date_buffer, "%FT%TZ", gmtime(&now));
			current_date_mutex.unlock();
			sleep(1000);
		}
    });
	    std::vector<std::thread *> threads(2);

    std::transform(threads.begin(), threads.end(), threads.begin(), [](std::thread */*t*/) {
        return new std::thread([]() {
			/* Overly simple hello world app */
			uWS::SSLApp({
			.key_file_name = "../misc/key.pem",
			.cert_file_name = "../misc/cert.pem",
			.passphrase = "1234"
			}).get("/*", [](auto *res, auto */*req*/) {
				current_date_mutex.lock();
				res->writeHeader("Date", current_date_buffer);
				current_date_mutex.unlock();
				
				res->writeHeader("Server", "uws");
				res->writeHeader("Content-Type", "text/plain");
				res->end("Hello, World!");
			}).listen(3000, [](auto *listen_socket) {
				if (listen_socket) {
					current_date_mutex.lock();

					std::cout << "Listening on port " << 3000 << std::endl;
					current_date_mutex.unlock();
				}
			}).run();
		});
    });

    std::for_each(threads.begin(), threads.end(), [](std::thread *t) {
        t->join();
    });}
