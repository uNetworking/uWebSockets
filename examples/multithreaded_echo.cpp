#include <uWS.h>
#include <iostream>
#include <thread>

#define THREADS 4
uWS::Hub *threadedServer[THREADS];

int main()
{
	// you need at least one server listening to a port
	uWS::Hub h;

	h.onConnection([&](uWS::WebSocket<uWS::SERVER> ws, uWS::HTTPRequest httpReq){
		int t = rand() % THREADS;
		std::cout << "Transferring connection to thread " << t << std::endl;
		ws.transfer(&threadedServer[t]->getDefaultGroup<uWS::SERVER>());
	});

	// launch the threads with their servers
	for (int i = 0; i < THREADS; i++) {
		new std::thread([i]{
			// register our events
			threadedServer[i] = new uWS::Hub();

			threadedServer[i]->onDisconnection([&i](uWS::WebSocket<uWS::SERVER> ws, int code, char *message, size_t length){
				std::cout << "Disconnection on thread " << i << std::endl;
			});

			threadedServer[i]->onMessage([&i](uWS::WebSocket<uWS::SERVER> ws, char *message, size_t length, uWS::OpCode code){
				ws.send((char *) message, length, code);
			});

			threadedServer[i]->getDefaultGroup<uWS::SERVER>().addAsync();
			threadedServer[i]->run();
		});

	}

	if (h.listen(3000)) {
		h.getDefaultGroup<uWS::SERVER>().addAsync();
		h.run();
	} else {
		std::cerr << "ERR_LISTEN" << std::endl;
	}

	return 0;
}
