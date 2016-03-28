/* this is the echo server being used in benchmarking */
/* it passes all Autobahn tests and doesn't use any fragment cheating */

#include "../uWS.h"
using namespace uWS;

int main()
{
    Server server(3000);
    server.onConnection([](Socket socket) {

    });

    server.onMessage([](Socket socket, const char *message, size_t length, OpCode opCode) {
        socket.send((char *) message, length, opCode);
    });

    server.onDisconnection([](Socket socket) {

    });

    server.run();
    return 0;
}
