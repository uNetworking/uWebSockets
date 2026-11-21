#include "../src/App.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <csignal>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

/* A dropped handler that ends the socket, with a client flooding PINGs and never reading.
 * The PONG is dropped, end() emits close, and no handler may be called after that. */

struct PerSocketData {};

static std::atomic<int> closes{0};
static std::atomic<int> droppedAfterClose{0};

static void flood(int port, uWS::Loop *loop, us_listen_socket_t **listenSocket) {
    int fd = (int) ::socket(AF_INET, SOCK_STREAM, 0);
    int small = 4096;
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &small, sizeof(small));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t) port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::connect(fd, (sockaddr *) &addr, sizeof(addr)) != 0) {
        std::cerr << "connect failed" << std::endl;
        std::abort();
    }

    std::string upgrade = "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                          "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n";
    ::send(fd, upgrade.data(), upgrade.size(), 0);
    std::string head;
    char c;
    while (head.find("\r\n\r\n") == std::string::npos && ::recv(fd, &c, 1, 0) == 1) {
        head += c;
    }

    /* Masked PINGs with a 125 byte payload, sent until the server stops taking them */
    std::string pings;
    for (int i = 0; i < 4096; i++) {
        pings += (char) 0x89;
        pings += (char) (0x80 | 125);
        pings += std::string(4, '\0');
        pings += std::string(125, 'p');
    }
    timeval tv{};
    tv.tv_sec = 1;
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    for (int i = 0; i < 200 && !closes; i++) {
        if (::send(fd, pings.data(), pings.size(), 0) <= 0) {
            break;
        }
    }
    /* Let the server go through what it has already read */
    usleep(200000);
    ::close(fd);

    loop->defer([listenSocket]() {
        us_listen_socket_close(0, *listenSocket);
    });
}

int main() {
    /* the client writes after the server ended the connection */
    signal(SIGPIPE, SIG_IGN);
    us_listen_socket_t *listenSocket = nullptr;
    int port = 0;

    uWS::App app;
    app.ws<PerSocketData>("/*", {
        .maxBackpressure = 16 * 1024,
        .dropped = [](auto *ws, std::string_view, uWS::OpCode) {
            if (closes) {
                droppedAfterClose++;
            }
            ws->end(1008);
        },
        .close = [](auto *, int, std::string_view) {
            closes++;
        }
    }).listen(0, [&](auto *token) {
        listenSocket = token;
        port = us_socket_local_port(0, (us_socket_t *) token);
    });

    if (!listenSocket) {
        std::cerr << "listen failed" << std::endl;
        return 1;
    }

    std::thread client(flood, port, uWS::Loop::get(), &listenSocket);
    app.run();
    client.join();

    if (closes != 1 || droppedAfterClose) {
        std::cerr << "closes: " << closes << ", dropped after close: " << droppedAfterClose << std::endl;
        return 1;
    }
    std::cout << "ALL PASS" << std::endl;
    return 0;
}
