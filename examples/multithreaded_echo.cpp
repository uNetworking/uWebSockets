#include <uWS.h>
#include <iostream>
#include <string>
#include <thread>
using namespace std;

using namespace uWS;

#define THREADS 4
uWS::Hub *threadedServer[THREADS];


int main()
{
	try {
		// you need at least one server listening to a port
		Hub h;
		
		h.onConnection([&](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req){
			int t = rand() % THREADS;
			cout << "Transfering connection to thread " << t << endl;
			ws.transfer(&threadedServer[t]->getDefaultGroup<uWS::SERVER>());
		});
		
		
		// launch the threads with their servers
		for (int i = 0; i < THREADS; i++) {
			new thread([i]{
				// register our events
				threadedServer[i] = new Hub();
				
				threadedServer[i]->onDisconnection([&i](WebSocket<uWS::SERVER> ws, int code, char *message, size_t length){
					cout << "Disconnection on thread " << i << endl;
				});
				
				threadedServer[i]->onMessage([&i](uWS::WebSocket<uWS::SERVER> ws, char *message, size_t length, uWS::OpCode code){
					cout << "Message on thread " << i << ": " << string(message, length) << endl;
					ws.send((char *) message, length, code);
				});
				
				threadedServer[i]->getDefaultGroup<uWS::SERVER>().addAsync();
				threadedServer[i]->run();
			});
			
		}
		
		h.getDefaultGroup<uWS::SERVER>().addAsync();
		h.listen(3000);
		h.run();
		
	} catch (...) {
		cout << "ERR_LISTEN" << endl;
	}
	
	return 0;
}
