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
        //cout << "Connections: " << ++connections << endl;
    });

    server.onFragment([](Socket socket, const char *fragment, size_t length, OpCode opCode, size_t remainingBytes) {
        //cout << "Fragment, length: " << length << ", remaining bytes: " << remainingBytes << endl;
        //cout << "Fragment: " << string(fragment, length) << endl;
        //socket.send((char *) fragment, length, binary);

        if (opCode == PONG) {
            //cout << "Got a pong!" << endl;
            opCode = PING;
        } else if (opCode == PING) {
            if (length + remainingBytes > 125) {
                cout << "Error: got too long control frame!" << endl;
                // in this case we should close the connection!
                // Case 2.5
                socket.fail();
                return;
            }
            opCode = PONG;
        }

        buffer.append(fragment, length);
        if (!remainingBytes) {
            unsigned long checksum = 0;
            for (unsigned char c : buffer) {
                checksum += c;
            }
            //cout << "Received buffer with checksum: " << checksum << endl;
            //cout << "Buffer length: " << buffer.length() << endl;

            socket.send((char *) buffer.c_str(), buffer.length(), opCode);
            buffer.clear();
        }

        //socket.sendFragment((char *) fragment, length, binary, remainingBytes);
    });

    server.onDisconnection([](Socket socket) {
        //cout << "Connections: " << --connections << endl;
    });

    server.run();
    cout << "Fell though!" << endl;
    return 0;
}
