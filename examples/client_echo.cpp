#include <iostream>
#include <string>
#include <thread>
#include <chrono>
using namespace std;

#include <uWS.h>
using namespace uWS;

int main()
{
	int port = 3000;
    try {
        Server server(port, true);
        server.onConnection([](ServerSocket socket) {
			cout << "Server received connection" << endl;
        });

        server.onMessage([](ServerSocket socket, const char *message, size_t length, OpCode opCode) {
			cout << "Server received message: " << string(message, length) << endl;
            socket.send((char *) message, length, opCode);
        });

        server.onDisconnection([](ServerSocket socket, int code, char *message, size_t length) {

        });

		Client client(true);
        client.onConnection([](ClientSocket socket) {
			cout << "Client connection success" << endl;
			string message = "hello world";
			socket.send((char *) message.c_str(), message.length(), OpCode::TEXT);
        });

        client.onConnectionFailure([]() {
			cout << "Client connection failure" << endl;
        });

        client.onMessage([](ClientSocket socket, const char *message, size_t length, OpCode opCode) {
			cout << "Client received message: " << string(message, length) << endl;
        });

        client.onDisconnection([](ClientSocket socket, int code, char *message, size_t length) {

        });
		client.connect("127.0.0.1", port);
		client.connect("127.0.0.1", port);

        server.run();
    } catch (std::runtime_error& e) {
        cout << "ERR_LISTEN" << e.what() << endl;
    }

    return 0;
}
