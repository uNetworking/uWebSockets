#include <iostream>
#include <string>
#include <thread>
#include <chrono>
using namespace std;

#include <uWS.h>
#include <uWS_Client.h>
using namespace uWS;

int main()
{
	int port = 3000;
    try {
        Server server(port, true);
        server.onConnection([](Socket socket) {
			cout << "Server received connection" << endl;
        });

        server.onMessage([](Socket socket, const char *message, size_t length, OpCode opCode) {
			cout << "Server received message: " << string(message, length) << endl;
            socket.send((char *) message, length, opCode);
        });

        server.onDisconnection([](Socket socket, int code, char *message, size_t length) {

        });

		uWS_Client::Client client("127.0.0.1", port, true);
        client.onConnectionSuccess([](uWS_Client::Socket socket) {
			cout << "Client connection success" << endl;
			string message = "hello world";
			socket.send((char *) message.c_str(), message.length(), uWS_Client::OpCode::TEXT);
        });

        client.onConnectionFailure([]() {
			cout << "Client connection failure" << endl;
        });

        client.onMessage([](uWS_Client::Socket socket, const char *message, size_t length, uWS_Client::OpCode opCode) {
			cout << "Client received message: " << string(message, length) << endl;
        });

        client.onDisconnection([](uWS_Client::Socket socket, int code, char *message, size_t length) {

        });
		client.connect();

        server.run();
    } catch (std::runtime_error& e) {
        cout << "ERR_LISTEN" << e.what() << endl;
    }

    return 0;
}
