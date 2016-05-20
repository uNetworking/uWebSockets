/* This is a very high performance general WebSocket server throughput benchmark */

#include <iostream>
#include <vector>
#include <chrono>
#include <uv.h>
#include <cstring>
#include <endian.h>
using namespace std;
using namespace chrono;

uv_loop_t *loop;
uv_buf_t upgradeHeader;
uv_buf_t framePack;

int byteSize, framesPerSend;
unsigned char *framePackBuffer;
int framePackBufferLength = 0;
const char upgradeHeaderBuffer[] = "GET / HTTP/1.1\r\n"
                                   "Upgrade: websocket\r\n"
                                   "Connection: Upgrade\r\n"
                                   "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n"
                                   "Host: server.example.com\r\n"
                                   "Sec-WebSocket-Version: 13\r\n\r\n";

int connections, remainingBytes;
vector<uv_stream_t *> sockets;
sockaddr_in addr;

unsigned long sent = 0;
auto startPoint = time_point<high_resolution_clock>();

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

        // Server does not send a mask of 4 bytes
        remainingBytes += framePackBufferLength - 4 * framesPerSend;
        sent += framesPerSend;
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
            exit(-1);
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
                    cout << "Connection error" << endl;
                    exit(-1);
                }
                delete write_t;
            });
        }
    });
}

int main(int argc, char *argv[])
{
    // Read arguments
    if (argc != 5) {
        cout << "Usage: throughput numberOfConnections payloadByteSize framesPerSend port" << endl;
        return -1;
    }

    connections = atoi(argv[1]);
    byteSize = atoi(argv[2]);
    framesPerSend = atoi(argv[3]);
    int port = atoi(argv[4]);

    // Init
    loop = uv_default_loop();
    uv_ip4_addr("127.0.0.1", port, &addr);
    upgradeHeader.base = (char *) upgradeHeaderBuffer;
    upgradeHeader.len = sizeof(upgradeHeaderBuffer) - 1;

    // Fill with random data
    int allocLength = (byteSize + 14) * framesPerSend;
    framePackBuffer = new unsigned char[allocLength];
    for (int i = 0; i < allocLength; i++) {
        framePackBuffer[i] = rand() % 255;
    }

    // Format message frame(s)
    unsigned char *framePackBufferOffset = framePackBuffer;
    for (int i = 0; i < framesPerSend; i++) {
        framePackBufferOffset[0] = 130;
        if (byteSize < 126) {
            framePackBufferLength += byteSize + 6;
            framePackBufferOffset[1] = 128 | byteSize;
            framePackBufferOffset += byteSize + 6;
        } else if (byteSize <= UINT16_MAX) {
            framePackBufferLength += byteSize + 8;
            framePackBufferOffset[1] = 128 | 126;
            *((uint16_t *) &framePackBufferOffset[2]) = htons(byteSize);
            framePackBufferOffset += byteSize + 8;
        } else {
            framePackBufferLength += byteSize + 14;
            framePackBufferOffset[1] = 128 | 127;
            *((uint64_t *) &framePackBufferOffset[2]) = htobe64(byteSize);
            framePackBufferOffset += byteSize + 14;
        }
    }

    framePack.base = (char *) framePackBuffer;
    framePack.len = framePackBufferLength;

    // Connect to echo server
    newConnection();
    return uv_run(loop, UV_RUN_DEFAULT);
}

