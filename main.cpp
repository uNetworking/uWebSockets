/* this is just a test example */

#include <iostream>
#include <string>
#include <thread>
using namespace std;

#include "uWS.h"
using namespace uWS;

int connections = 0;
Server *worker, *server;

int main()
{
    try {
        // set up ssl
        SSLContext sslContext("/home/alexhultman/uws-connections-dropped/secrets/cert.pem",
                              "/home/alexhultman/uws-connections-dropped/secrets/key.pem");

        // our listening server
        Server server(3000, false, 0, 0, sslContext);
        server.onUpgrade([](uv_os_sock_t fd, const char *secKey, void *ssl, const char *extensions, size_t extensionsLength) {
            // transfer connection to one of our worker servers
            ::worker->upgrade(fd, secKey, ssl, extensions, extensionsLength);
        });

        // our working server, does not listen
        Server worker(0, false, PERMESSAGE_DEFLATE | SERVER_NO_CONTEXT_TAKEOVER | CLIENT_NO_CONTEXT_TAKEOVER, 0);
        ::worker = &worker;
        ::server = &server;
        worker.onConnection([](WebSocket socket) {
            cout << "[Connection] clients: " << ++connections << endl;

            //socket.ping();

            //socket.close();
            //socket.close(false, 1011, "abcd", 4);

            WebSocket::Address a = socket.getAddress();
            cout << a.address << endl;
            cout << a.family << endl;
            cout << a.port << endl;

            //char message[] = "Welcome!";
            //::worker->broadcast(message, sizeof(message) - 1, OpCode::TEXT);

            // test shutting down the server when two clients are connected
            // this should disconnect both clients and exit libuv loop
            /*if (connections == 2) {
                ::worker->broadcast("I'm shutting you down now", 25, TEXT);
                cout << "Shutting down server now" << endl;
                ::worker->close();
                ::server->close();
            }*/
        });

        worker.onPing([](WebSocket webSocket, char *message, size_t length) {
            cout << "Got a ping!" << endl;
        });

        worker.onPong([](WebSocket webSocket, char *message, size_t length) {
            cout << "Got a pong!" << endl;
        });

        worker.onMessage([](WebSocket socket, char *message, size_t length, OpCode opCode) {
            socket.send(message, length, opCode/*, [](WebSocket webSocket, void *data, bool cancelled) {
                cout << "Sent: " << (char *) data << endl;
            }, (void *) "Some callback data here"*/);
        });

        worker.onDisconnection([](WebSocket socket, int code, char *message, size_t length) {
            cout << "[Disconnection] clients: " << --connections << endl;
            cout << "Code: " << code << endl;
            cout << "Message: " << string(message, length) << endl;

            static int numDisconnections = 0;
            numDisconnections++;
            cout << numDisconnections << endl;
            if (numDisconnections == 519) {
                cout << "Closing after Autobahn test" << endl;
                ::worker->close();
                ::server->close();
            }
        });

        // work on other thread
        thread *t = new thread([]() {
            ::worker->run();
        });

        // listen on main thread
        server.run();
        t->join();
        delete t;
    } catch (...) {
        cout << "ERR_LISTEN" << endl;
    }

    return 0;
}
