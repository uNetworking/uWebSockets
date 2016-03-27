#include <iostream>
#include <string>
using namespace std;

#include "uWS.h"
using namespace uWS;

int connections = 0;
string buffer;
string controlBuffer;

Server *server;

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

                // Chapter 6
                if (opCode == 1 && !Server::isValidUtf8(buffer)) {
                    cout << "Connections: " << --connections << endl;
                    socket.fail();
                    return;
                }

                socket.send((char *) buffer.c_str(), buffer.length(), opCode);
                buffer.clear();
            }
        } else {

            // this part is control frame, should be handeled in the lib?

            if (!fin || (length + remainingBytes > 125)) {
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
    ::server = &server;
    server.onDisconnection([](Socket socket) {
        cout << "Connections: " << --connections << endl;

        buffer.clear();
        controlBuffer.clear();

        static int numDisconnections = 0;
        numDisconnections++;
        if (numDisconnections == 224) {
            cout << "Closing after Autobahn test" << endl;
            ::server->close();
        }
    });

    server.run();
    cout << "Fell through!" << endl;
    return 0;
}
