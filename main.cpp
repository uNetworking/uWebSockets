/* this is just a test example */

#include <iostream>
#include <string>
using namespace std;

#include "uWS.h"
using namespace uWS;

int connections = 0;
Server *server;

int main()
{
    try {
        Server server(3000);
        server.onConnection([](Socket socket) {
            cout << "[Connection] clients: " << ++connections << endl;

            // test shutting down the server when two clients are connected
            // this should disconnect both clients and exit libuv loop
            if (connections == 2) {
                ::server->broadcast("I'm shutting you down now", 25, TEXT);
                cout << "Shutting down server now" << endl;
                ::server->close();
            }
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
    } catch (...) {
        cout << "ERR_LISTEN" << endl;
    }

    return 0;
}
