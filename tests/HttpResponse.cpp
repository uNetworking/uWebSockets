#include "../src/App.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

/* Regression tests for chunked response framing (issue #1939).
 * beginWrite() must terminate headers once; write()/end() must emit
 * complete chunks (size CRLF data CRLF) without a leading extra CRLF. */

static void dump(std::string_view s) {
    for (unsigned char c : s) {
        if (c == '\r') {
            std::cerr << "\\r";
        } else if (c == '\n') {
            std::cerr << "\\n";
        } else {
            std::cerr << (char) c;
        }
    }
}

static std::string request(int port, const char *path) {
    int fd = (int) ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        std::cerr << "socket failed: " << std::strerror(errno) << std::endl;
        std::abort();
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t) port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    int connected = -1;
    for (int i = 0; i < 50; i++) {
        connected = ::connect(fd, (sockaddr *) &addr, sizeof(addr));
        if (connected == 0) {
            break;
        }
        ::close(fd);
        fd = (int) ::socket(AF_INET, SOCK_STREAM, 0);
        usleep(10000);
    }

    if (connected != 0) {
        std::cerr << "connect failed: " << std::strerror(errno) << std::endl;
        std::abort();
    }

    timeval tv{};
    tv.tv_sec = 2;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    std::string req = std::string("GET ") + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    if (::send(fd, req.data(), req.size(), 0) != (ssize_t) req.size()) {
        std::cerr << "send failed: " << std::strerror(errno) << std::endl;
        std::abort();
    }

    std::string response;
    char buf[2048];
    while (true) {
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n > 0) {
            response.append(buf, (size_t) n);
            continue;
        }
        if (n == 0) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        std::cerr << "recv failed: " << std::strerror(errno) << std::endl;
        std::cerr << "partial response: ";
        dump(response);
        std::cerr << std::endl;
        std::abort();
    }

    ::close(fd);
    return response;
}

/* Send the request but do not read until the server has finished writing.
 * Small SO_RCVBUF so the sender hits backpressure (write() returns false). */
static std::string requestAfterBackpressure(int port, const char *path, std::atomic<bool> &written) {
    int fd = (int) ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        std::cerr << "socket failed: " << std::strerror(errno) << std::endl;
        std::abort();
    }

    int rcv = 4096;
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcv, sizeof(rcv));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t) port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    int connected = -1;
    for (int i = 0; i < 50; i++) {
        connected = ::connect(fd, (sockaddr *) &addr, sizeof(addr));
        if (connected == 0) {
            break;
        }
        ::close(fd);
        fd = (int) ::socket(AF_INET, SOCK_STREAM, 0);
        setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcv, sizeof(rcv));
        usleep(10000);
    }

    if (connected != 0) {
        std::cerr << "connect failed: " << std::strerror(errno) << std::endl;
        std::abort();
    }

    std::string req = std::string("GET ") + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    if (::send(fd, req.data(), req.size(), 0) != (ssize_t) req.size()) {
        std::cerr << "send failed: " << std::strerror(errno) << std::endl;
        std::abort();
    }

    for (int i = 0; i < 500 && !written.load(); i++) {
        usleep(10000);
    }
    if (!written.load()) {
        std::cerr << "server did not finish backpressure writes" << std::endl;
        std::abort();
    }

    timeval tv{};
    tv.tv_sec = 10;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    std::string response;
    char buf[8192];
    while (true) {
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n > 0) {
            response.append(buf, (size_t) n);
            continue;
        }
        if (n == 0) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        std::cerr << "recv failed: " << std::strerror(errno) << std::endl;
        std::abort();
    }

    ::close(fd);
    return response;
}

static std::string_view bodyOf(const std::string &response) {
    size_t pos = response.find("\r\n\r\n");
    if (pos == std::string::npos) {
        std::cerr << "missing header terminator in: ";
        dump(response);
        std::cerr << std::endl;
        std::abort();
    }
    return std::string_view(response).substr(pos + 4);
}

/* Walk RFC 9112 chunks. Fails if any chunk-data is missing its trailing CRLF. */
static std::string decodeChunked(std::string_view body) {
    std::string out;
    size_t i = 0;

    while (i < body.size()) {
        size_t lineEnd = body.find("\r\n", i);
        if (lineEnd == std::string_view::npos) {
            std::cerr << "chunk-size line not terminated at offset " << i << "\nbody tail: ";
            dump(body.substr(i, 32));
            std::cerr << std::endl;
            std::abort();
        }

        unsigned long size = 0;
        for (size_t j = i; j < lineEnd; j++) {
            char c = body[j];
            size <<= 4;
            if (c >= '0' && c <= '9') {
                size += (unsigned long) (c - '0');
            } else if (c >= 'a' && c <= 'f') {
                size += (unsigned long) (c - 'a' + 10);
            } else {
                std::cerr << "invalid chunk-size hex at offset " << j << std::endl;
                std::abort();
            }
        }

        i = lineEnd + 2;
        if (size == 0) {
            if (body.substr(i) != "\r\n") {
                std::cerr << "bad last-chunk trailer: ";
                dump(body.substr(i));
                std::cerr << std::endl;
                std::abort();
            }
            return out;
        }

        if (i + size + 2 > body.size()) {
            std::cerr << "truncated chunk of size " << size << " at offset " << i << std::endl;
            std::abort();
        }

        if (body[i + size] != '\r' || body[i + size + 1] != '\n') {
            std::cerr << "missing chunk-data CRLF after " << size
                      << " byte chunk (backpressure write skipped the trailer)\nnext bytes: ";
            dump(body.substr(i + size, 16));
            std::cerr << std::endl;
            std::abort();
        }

        out.append(body.data() + i, size);
        i += size + 2;
    }

    std::cerr << "missing terminating 0 chunk" << std::endl;
    std::abort();
}

static void expectChunked(int port, const char *path, std::string_view expectedBody) {
    std::string response = request(port, path);

    if (response.find("Transfer-Encoding: chunked\r\n") == std::string::npos) {
        std::cerr << path << " missing Transfer-Encoding: chunked\n";
        dump(response);
        std::cerr << std::endl;
        std::abort();
    }

    std::string_view body = bodyOf(response);
    if (body != expectedBody) {
        std::cerr << path << " unexpected chunked body\nexpected: ";
        dump(expectedBody);
        std::cerr << "\nactual:   ";
        dump(body);
        std::cerr << std::endl;
        std::abort();
    }

    std::cout << "OK " << path << std::endl;
}

int main() {
    uWS::App app;
    uWS::Loop *loop = uWS::Loop::get();
    int port = 0;
    std::atomic<bool> backpressureWritten{false};
    constexpr int chunkSize = 64 * 1024;
    constexpr int maxFillChunks = 512;

    app.get("/write-end", [](auto *res, auto * /*req*/) {
        res->onAborted([]() {});
        res->cork([res]() {
            res->writeHeader("Content-Type", "text/plain");
            res->write("foo");
            res->end();
        });
    }).get("/begin-write-end", [](auto *res, auto * /*req*/) {
        res->onAborted([]() {});
        res->cork([res]() {
            res->writeHeader("Content-Type", "text/plain");
            res->beginWrite();
            res->write("foo");
            res->end();
        });
    }).get("/begin-end", [](auto *res, auto * /*req*/) {
        res->onAborted([]() {});
        res->cork([res]() {
            res->writeHeader("Content-Type", "text/plain");
            res->beginWrite();
            res->end();
        });
    }).get("/begin-end-data", [](auto *res, auto * /*req*/) {
        res->onAborted([]() {});
        res->cork([res]() {
            res->writeHeader("Content-Type", "text/plain");
            res->beginWrite();
            res->end("foo");
        });
    }).get("/write-write-end", [](auto *res, auto * /*req*/) {
        res->onAborted([]() {});
        res->cork([res]() {
            res->writeHeader("Content-Type", "text/plain");
            res->write("foo");
            res->write("bar");
            res->end();
        });
    }).get("/begin-write-write-end", [](auto *res, auto * /*req*/) {
        res->onAborted([]() {});
        res->cork([res]() {
            res->writeHeader("Content-Type", "text/plain");
            res->beginWrite();
            res->write("foo");
            res->write("bar");
            res->end();
        });
    }).get("/write-end-data", [](auto *res, auto * /*req*/) {
        res->onAborted([]() {});
        res->cork([res]() {
            res->writeHeader("Content-Type", "text/plain");
            res->write("foo");
            res->end("bar");
        });
    }).get("/empty-write", [](auto *res, auto * /*req*/) {
        res->onAborted([]() {});
        res->cork([res]() {
            res->writeHeader("Content-Type", "text/plain");
            res->beginWrite();
            res->write("");
            res->end();
        });
    }).get("/hex-size", [](auto *res, auto * /*req*/) {
        res->onAborted([]() {});
        res->cork([res]() {
            res->writeHeader("Content-Type", "text/plain");
            res->beginWrite();
            res->write("hello world");
            res->end();
        });
    }).get("/backpressure", [&](auto *res, auto * /*req*/) {
        res->onAborted([]() {});
        res->cork([res]() {
            res->writeHeader("Content-Type", "text/plain");
            std::string chunk((size_t) chunkSize, 'a');
            int filled = 0;
            while (res->write(chunk)) {
                if (++filled >= maxFillChunks) {
                    std::cerr << "did not hit backpressure after " << filled << " writes" << std::endl;
                    std::abort();
                }
            }
            /* This write is also backpressured: data is queued, write() returns false.
             * The chunk trailer must still be present on the wire. */
            res->write("foo");
            res->end();
        });
        backpressureWritten.store(true);
    }).listen(0, [&](us_listen_socket_t *listenSocket) {
        if (!listenSocket) {
            std::cerr << "Failed to listen" << std::endl;
            std::exit(1);
        }
        port = us_socket_local_port(0, (us_socket_t *) listenSocket);
    });

    if (port <= 0) {
        std::cerr << "listen returned no port" << std::endl;
        return 1;
    }

    std::thread client([loop, &app, port, &backpressureWritten]() {
        expectChunked(port, "/write-end", "3\r\nfoo\r\n0\r\n\r\n");
        expectChunked(port, "/begin-write-end", "3\r\nfoo\r\n0\r\n\r\n");
        expectChunked(port, "/begin-end", "0\r\n\r\n");
        expectChunked(port, "/begin-end-data", "3\r\nfoo\r\n0\r\n\r\n");
        expectChunked(port, "/write-write-end", "3\r\nfoo\r\n3\r\nbar\r\n0\r\n\r\n");
        expectChunked(port, "/begin-write-write-end", "3\r\nfoo\r\n3\r\nbar\r\n0\r\n\r\n");
        expectChunked(port, "/write-end-data", "3\r\nfoo\r\n3\r\nbar\r\n0\r\n\r\n");
        expectChunked(port, "/empty-write", "0\r\n\r\n");
        expectChunked(port, "/hex-size", "b\r\nhello world\r\n0\r\n\r\n");

        std::string response = requestAfterBackpressure(port, "/backpressure", backpressureWritten);
        if (response.find("Transfer-Encoding: chunked\r\n") == std::string::npos) {
            std::cerr << "/backpressure missing Transfer-Encoding: chunked\n";
            std::abort();
        }
        std::string decoded = decodeChunked(bodyOf(response));
        if (decoded.size() < (size_t) chunkSize + 3 || decoded.compare(decoded.size() - 3, 3, "foo") != 0) {
            std::cerr << "/backpressure decoded body should be N*'a' + \"foo\", got size "
                      << decoded.size() << std::endl;
            std::abort();
        }
        for (size_t i = 0; i + 3 < decoded.size(); i++) {
            if (decoded[i] != 'a') {
                std::cerr << "/backpressure unexpected byte at " << i << std::endl;
                std::abort();
            }
        }
        std::cout << "OK /backpressure (" << decoded.size() << " decoded bytes)" << std::endl;

        loop->defer([&app]() {
            app.close();
        });
    });

    app.run();
    client.join();
    return 0;
}
