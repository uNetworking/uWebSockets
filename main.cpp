#include <iostream>
#include <string>
using namespace std;

#include "uWS.h"
using namespace uWS;

int connections = 0;
string buffer;

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


        buffer.append(fragment, length);
        if (!remainingBytes) {
            unsigned long checksum = 0;
            for (unsigned char c : buffer) {
                checksum += c;
            }
            cout << "Received buffer with checksum: " << checksum << endl;
            socket.send((char *) buffer.c_str(), buffer.length(), binary);
            buffer.clear();
        }

        //socket.sendFragment((char *) fragment, length, binary, remainingBytes);
    });

    server.onDisconnection([](Socket socket) {
        cout << "Connections: " << --connections << endl;
    });

    server.run();
    cout << "Fell though!" << endl;
    return 0;
}
