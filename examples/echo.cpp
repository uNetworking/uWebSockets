/* this is an echo server that properly passes every supported Autobahn test */

#include <iostream>
#include <string>
using namespace std;

#include <uWS.h>
using namespace uWS;

int main()
{
    try {
        Server server(3000, true, PERMESSAGE_DEFLATE, 0);
        server.onConnection([](WebSocket socket) {

        });

        server.onMessage([](WebSocket socket, char *message, size_t length, OpCode opCode) {
            socket.send(message, length, opCode);
        });

        server.onDisconnection([](WebSocket socket, int code, char *message, size_t length) {

        });

        server.run();
    } catch (...) {
        cout << "ERR_LISTEN" << endl;
    }

    return 0;
}
