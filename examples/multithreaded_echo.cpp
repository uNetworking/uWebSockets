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
		// The main hub, used to listen for connections.
		Hub h;
		
		h.onConnection([&](uWS::WebSocket<uWS::SERVER> ws, uWS::UpgradeInfo upgInfo){
			int t = rand() % THREADS;
			cout << "Transferring connection to thread " << t << endl;
			ws.transfer(&threadedServer[t]->getDefaultGroup<uWS::SERVER>());
		});
		
		
		// launch the threads with their servers
		for (int i = 0; i < THREADS; i++) {
			new thread([i]{
				threadedServer[i] = new Hub();
				
				// register our events
				threadedServer[i]->onDisconnection([&i](WebSocket<uWS::SERVER> ws, int code, char *message, size_t length){
					cout << "Disconnection on thread " << i << endl;
				});
				
				threadedServer[i]->onMessage([&i](uWS::WebSocket<uWS::SERVER> ws, char *message, size_t length, uWS::OpCode code){
					cout << "Message on thread " << i << ": " << string(message, length) << endl;
					ws.send((char *) message, length, code);
				});
				
				// Tell the threaded hub to listen for transfers from other threads
				threadedServer[i]->getDefaultGroup<uWS::SERVER>().addAsync();
				threadedServer[i]->run();
			});
			
		}
		
		h.listen(3000);
		h.run();
		
	} catch (...) {
		cout << "ERR_LISTEN" << endl;
	}
	
	return 0;
}
