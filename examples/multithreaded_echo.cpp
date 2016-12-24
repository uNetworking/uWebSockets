#include <uWS.h>
#include <iostream>
#include <string>
#include <thread>
#include <sstream>
#include <iomanip>
using namespace std;

using namespace uWS;

#define THREADS 4
uWS::Hub *threadedServer[THREADS];

static string hexDump(char *buf, size_t size);

int main()
{
	// you need at least one server listening to a port
	Hub h;
	
	h.onConnection([&](uWS::WebSocket<uWS::SERVER> ws, uWS::HTTPRequest httpReq){
		int t = rand() % THREADS;
		cout << "Transferring connection to thread " << t << endl;
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
				//cerr << "Message on thread " << i << ": " << hexDump(message, length) << endl;
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
		cerr << "ERR_LISTEN" << endl;
	}
	
	return 0;
}

string hexDump(char *buf, size_t size) {
    if (buf == nullptr) {
        return "";
    }
    static const char hexChars[] = "0123456789abcdef";
    stringstream ss;
    for (size_t i = 0; i < size; i++) {
        ss << "\\x" << hexChars[(buf[i] >> 4) & 0xf] << hexChars[buf[i] & 0xf];
    }
    return ss.str();
}
