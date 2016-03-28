/* this is an example echo server that passes the Autobahn tests */
/* it shows the three basic events and how to send messages back */

#include <iostream>
#include <string>
using namespace std;

#include "uWS.h"
using namespace uWS;

int connections = 0;
Server *server;

int main()
{
    Server server(3000);
    server.onConnection([](Socket socket) {
        cout << "[Connection] clients: " << ++connections << endl;
    });

    server.onMessage([](Socket socket, const char *message, size_t length, OpCode opCode) {
        socket.send((char *) message, length, opCode);
    });

    ::server = &server;
    server.onDisconnection([](Socket socket) {
        cout << "[Disconnection] clients: " << --connections << endl;

        static int numDisconnections = 0;
        numDisconnections++;
        cout << numDisconnections << endl;
        if (numDisconnections == 302) {
            cout << "Closing after Autobahn test" << endl;
            ::server->close();
        }
    });

    server.run();
    return 0;
}
