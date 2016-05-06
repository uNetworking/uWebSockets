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

        server.onUpgrade([](FD fd, const char *secKey) {
            // we transfer the connection to one of the other servers
            threadedServer[rand() % THREADS]->upgrade(fd, secKey);
        });

        // launch the threads with their servers
        for (int i = 0; i < THREADS; i++) {
            new thread([i]{
                threadedServer[i] = new Server(0);

                // register our events
                threadedServer[i]->onConnection([i](Socket socket) {
                    cout << "Connection on thread " << i << endl;
                });

                threadedServer[i]->onDisconnection([i](Socket socket, int code, char *message, size_t length) {
                    cout << "Disconnection on thread " << i << endl;
                });

                threadedServer[i]->onMessage([i](Socket socket, const char *message, size_t length, OpCode opCode) {
                    cout << "Message on thread " << i << ": " << string(message, length) << endl;
                    socket.send((char *) message, length, opCode);
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
