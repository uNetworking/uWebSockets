/* This is an optimized version of bench2, since I cannot stress µWebSockets
 * fully even if I launch 3 processes of WebSocket++ clients, that's why I need
 * this crazily optimized WebSocket client to truly stress the server */

#include <iostream>
#include <vector>
#include <chrono>
#include <uv.h>
#include <cstring>
using namespace std;
using namespace chrono;

// Upgrade header
const char buf[] = "GET / HTTP/1.1\r\n"
                  "Host: server.example.com\r\n"
                  "Upgrade: websocket\r\n"
                  "Connection: Upgrade\r\n"
                  "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n"
                  "Sec-WebSocket-Protocol: default\r\n"
                  "Sec-WebSocket-Version: 13\r\n"
                  "Origin: http://example.com\r\n\r\n";

// Pre-masked set of 10 packed frames with payload "This will be echoed"
char data[] = {-126, -109, -94, -39, 5, 19, -10, -79, 108, 96, -126, -82, 108, 127, -50, -7, 103, 118, -126, -68, 102, 123,
               -51, -68, 97, -126, -109, -94, -90, 0, -40, -10, -50, 105, -85, -126, -47, 105, -76, -50, -122, 98, -67, -126,
               -61, 99, -80, -51, -61, 100, -126, -109, -39, -13, -115, 75, -115, -101, -28, 56, -7, -124, -28, 39, -75, -45, -17,
               46, -7, -106, -18, 35, -74, -106, -23, -126, -109, -72, -5, 50, 47, -20, -109, 91, 92, -104, -116, 91, 67, -44,
               -37, 80, 74, -104, -98, 81, 71, -41, -98, 86, -126, -109, -30, -25, 6, 113, -74, -113, 111, 2, -62, -112, 111, 29,
               -114, -57, 100, 20, -62, -126, 101, 25, -115, -126, 98, -126, -109, 5, 80, -123, 20, 81, 56, -20, 103, 37, 39,
               -20, 120, 105, 112, -25, 113, 37, 53, -26, 124, 106, 53, -31, -126, -109, -6, 91, -114, -52, -82, 51, -25, -65, -38,
               44, -25, -96, -106, 123, -20, -87, -38, 62, -19, -92, -107, 62, -22, -126, -109, 127, 8, 62, -79, 43, 96, 87, -62, 95,
               127, 87, -35, 19, 40, 92, -44, 95, 109, 93, -39, 16, 109, 90, -126, -109, -43, -124, -22, 48, -127, -20, -125, 67,
               -11, -13, -125, 92, -71, -92, -120, 85, -11, -31, -119, 88, -70, -31, -114, -126, -109, -14, 12, 77, -10, -90,
               100, 36, -123, -46, 123, 36, -102, -98, 44, 47, -109, -46, 105, 46, -98, -99, 105, 41};

uv_buf_t upgradeHeader;
uv_buf_t framePack;
uv_loop_t *loop;

int connections, remainingBytes;
vector<uv_stream_t *> sockets;
sockaddr_in addr;

unsigned long sent = 0;
auto startPoint = high_resolution_clock::now();

void echo()
{
    for (int i = 0; i < connections; i++) {
        // Write message on random socket
        uv_write(new uv_write_t, sockets[rand() % connections], &framePack, 1, [](uv_write_t *write_t, int status) {
            if (status < 0) {
                cout << "Write error" << endl;
                exit(0);
            }
            delete write_t;
        });

        // Each wiriting should result in 210 bytes of received data
        remainingBytes += 210;
        sent += 10;
    }
}

void newConnection()
{
    uv_tcp_t *socket = new uv_tcp_t;
    socket->data = nullptr;
    uv_tcp_init(loop, socket);

    uv_tcp_connect(new uv_connect_t, socket, (sockaddr *) &addr, [](uv_connect_t *connect, int status) {
        if (status < 0) {
            cout << "Connection error" << endl;
            exit(0);
        } else {
            uv_read_start(connect->handle, [](uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
                buf->base = new char[suggested_size];
                buf->len = suggested_size;
            }, [](uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {

                if (stream->data) {
                    // If we now received all bytes we expect from last echoing, echo again
                    remainingBytes -= nread;
                    if (!remainingBytes) {
                        cout << "Echo performance: " << double(sent) / (1e-3 * duration_cast<microseconds>(high_resolution_clock::now() - startPoint).count()) << " echoes/ms" << endl;
                        echo();
                    }
                } else {
                    // WebSocket connection established here
                    stream->data = (void *) 1;
                    sockets.push_back(stream);

                    cout << "Connections: " << sockets.size() << endl;

                    // Perform first batch of echo sending
                    if (sockets.size() == connections) {
                        startPoint = high_resolution_clock::now();
                        echo();
                    } else {
                        newConnection();
                    }
                }
                delete [] buf->base;
            });

            // Send upgrade header
            uv_write(new uv_write_t, connect->handle, &upgradeHeader, 1, [](uv_write_t *write_t, int status) {
                if (status < 0) {
                    cout << "Write error" << endl;
                } else {

                }
                delete write_t;
            });
        }
    });
}

int main(int argc, char *argv[])
{
    // Read arguments (number of connections, port)
    if (argc != 3) {
        cout << "Usage: bench2_optimized numberOfConnections port" << endl;
        return -1;
    }

    connections = atoi(argv[1]);
    int port = atoi(argv[2]);

    // Init
    loop = uv_default_loop();
    uv_ip4_addr("127.0.0.1", port, &addr);
    upgradeHeader.base = (char *) buf;
    upgradeHeader.len = sizeof(buf) - 1;
    framePack.base = data;
    framePack.len = 250;

    // Connect to echo server
    newConnection();

    return uv_run(loop, UV_RUN_DEFAULT);
}

