#include <iostream>
using namespace std;

#include "uWS.h"
using namespace uWS;

int connections = 0;

#include <unistd.h>

int main()
{
    Server server(3000);

    server.onConnection([](Socket socket) {
        cout << "Connections: " << ++connections << endl;
    });

    server.onFragment([](Socket socket, const char *fragment, size_t length, bool binary, size_t remainingBytes) {
        //cout << "Fragment, length: " << length << ", remaining bytes: " << remainingBytes << endl;
        //cout << "Fragment: " << string(fragment, length) << endl;
        //socket.send((char *) fragment, length, binary);

        // limit sending (we do not properly wait for UV_WRITABLE yet)
        //usleep(1);

        socket.sendFragment((char *) fragment, length, binary, remainingBytes);
    });

    server.onDisconnection([](Socket socket) {
        cout << "Connections: " << --connections << endl;
    });

    server.run();
    return 0;
}
