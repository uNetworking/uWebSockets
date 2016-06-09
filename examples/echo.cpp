/* this is an echo server that properly passes every supported Autobahn test */

#include <iostream>
#include <string>
using namespace std;

#include <uWS.h>
using namespace uWS;

int main()
{
    try {
        Server server(3000);
        server.onConnection([](ServerSocket socket) {

        });

        server.onMessage([](ServerSocket socket, const char *message, size_t length, OpCode opCode) {
            socket.send((char *) message, length, opCode);
        });

        server.onDisconnection([](ServerSocket socket, int code, char *message, size_t length) {

        });

        server.run();
    } catch (...) {
        cout << "ERR_LISTEN" << endl;
    }

    return 0;
}
