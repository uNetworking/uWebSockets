#include <iostream>
#include <string>
using namespace std;

#include "uWS.h"
using namespace uWS;

int connections = 0;
string buffer;
string controlBuffer;

int main()
{
    Server server(3000);

    server.onConnection([](Socket socket) {
        cout << "Connections: " << ++connections << endl;
        buffer.clear();
        controlBuffer.clear();
    });

    server.onFragment([](Socket socket, const char *fragment, size_t length, OpCode opCode, bool fin, size_t remainingBytes) {
        //cout << "Fragment, length: " << length << ", remaining bytes: " << remainingBytes << endl;
        //cout << "Fragment: " << string(fragment, length) << ", opCode: " << opCode << ", fin: " << fin << ", remainingBytes: " << remainingBytes << endl;
        //socket.send((char *) fragment, length, binary);

        // Text or binary
        if (opCode < 3) {
            buffer.append(fragment, length);
            if (!remainingBytes && fin) {
                //cout << "Sending: " << buffer << endl;
                socket.send((char *) buffer.c_str(), buffer.length(), opCode);
                buffer.clear();
            }
        } else {
            // if fragmented, terminate
            if (!fin) {
                cout << "Connections: " << --connections << endl;
                socket.fail();
                return;
            }

            if (length + remainingBytes > 125) {
                cout << "Error: got too long control frame!" << endl;
                // in this case we should close the connection!
                // Case 2.5
                cout << "Connections: " << --connections << endl;
                socket.fail();
                return;
            }

            // swap PING/PONG
            if (opCode == PING) {
                opCode = PONG;
            } else if (opCode == PONG) {
                opCode = PING;
            }

            // append to a separate buffer for control messages
            controlBuffer.append(fragment, length);
            if (!remainingBytes && fin) {
                socket.send((char *) controlBuffer.c_str(), controlBuffer.length(), opCode);
                controlBuffer.clear();
            }
        }

//        //socket.sendFragment((char *) fragment, length, binary, remainingBytes);
    });

    // Socket.fail does not call this one!
    server.onDisconnection([](Socket socket) {
        cout << "Connections: " << --connections << endl;
    });

    server.run();
    cout << "Fell though!" << endl;
    return 0;
}
