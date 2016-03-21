#include <iostream>
using namespace std;

#include "uWS.h"
using namespace uWS;

int connections = 0;

int main()
{
    Server server(3000);

    server.onConnection([](Socket socket) {
        cout << "Connections: " << ++connections << endl;
    });

    server.onFragment([](Socket socket, const char *fragment, size_t length) {

        cout << "Fragment, length: " << length << endl;

        //cout << "Fragment: " << string(fragment, length) << endl;
        socket.send((char *) fragment, length);
    });

    server.onDisconnection([](Socket socket) {
        cout << "Connections: " << --connections << endl;
    });

    server.run();
    return 0;
}
