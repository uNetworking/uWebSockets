/* this example shows how you can scale vertically over all CPU cores */
/* threading is up to the user, so you could listen to one port or a range */
/* depending on what you want, I don't care what you chose to do */

#include <iostream>
#include <string>
#include <thread>
using namespace std;

#include <uWS.h>
using namespace uWS;

#define THREADS 4
Server *threadedServer[THREADS];

int main()
{
    try {
        // you need at least one server listening to a port
        Server server(3000);

        server.onUpgrade([](uv_os_fd_t fd, const char *secKey, void *ssl, const char *extensions, size_t extensionsLength) {
            // we transfer the connection to one of the other servers
            threadedServer[rand() % THREADS]->upgrade(fd, secKey, ssl, extensions, extensionsLength);
        });

        // launch the threads with their servers
        for (int i = 0; i < THREADS; i++) {
            new thread([i]{
                threadedServer[i] = new Server(0, false);

                // register our events
                threadedServer[i]->onConnection([i](WebSocket socket) {
                    cout << "Connection on thread " << i << endl;
                });

                threadedServer[i]->onDisconnection([i](WebSocket socket, int code, char *message, size_t length) {
                    cout << "Disconnection on thread " << i << endl;
                });

                threadedServer[i]->onMessage([i](WebSocket socket, char *message, size_t length, OpCode opCode) {
                    cout << "Message on thread " << i << ": " << string(message, length) << endl;
                    socket.send(message, length, opCode);
                });

                threadedServer[i]->run();
            });
        }

        // run listener
        server.run();
    } catch (...) {
        cout << "ERR_LISTEN" << endl;
    }

    return 0;
}
