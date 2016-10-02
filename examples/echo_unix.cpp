/* this is an echo server that properly passes every supported Autobahn test */

#include <iostream>
#include <string>
using namespace std;

#include <uWS.h>
using namespace uWS;

// to test, use: socat TCP-LISTEN:3000,fork UNIX-CONNECT:test_socket
// then connect with WebSocket client to port 3000
int main()
{
    try {
        EventSystem es(MASTER);
        Server server(es, "test_socket", PERMESSAGE_DEFLATE, 0);
        server.onConnection([](WebSocket socket) {

        });

        server.onMessage([](WebSocket socket, char *message, size_t length, OpCode opCode) {
            socket.send(message, length, opCode);
        });

        server.onDisconnection([](WebSocket socket, int code, char *message, size_t length) {

        });

        es.run();
    } catch (...) {
        cout << "ERR_LISTEN" << endl;
    }

    return 0;
}
