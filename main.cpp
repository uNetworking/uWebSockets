#include <iostream>
using namespace std;

#include "uWS.h"
using namespace uWS;

int connections = 0;

int main()
{
    Server server(3000);

    server.onConnection([](Socket socket) {
        cout << "Connection" << endl;
        cout << "Connections: " << ++connections << endl;
    });

    server.onFragment([](Socket socket, const char *fragment, size_t length, bool binary, size_t remainingBytes) {
        //cout << "Fragment, length: " << length << ", remaining bytes: " << remainingBytes << endl;
        //cout << "Fragment: " << string(fragment, length) << endl;
        //socket.send((char *) fragment, length, binary);

        socket.sendFragment((char *) fragment, length, binary, remainingBytes);
    });

    server.onDisconnection([](Socket socket) {
        cout << "Disconnection" << endl;
        cout << "Connections: " << --connections << endl;
    });

    server.run();
    return 0;
}
