#include <iostream>
#include <cassert>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "../src/App.h"
#include "../uSockets/src/libusockets.h"

/* Global test state */
bool testPassed = false;
std::string receivedResponse;

void testBeginWrite() {
    std::cout << "TestBeginWrite" << std::endl;
    
    us_listen_socket_t *listenSocket = nullptr;
    
    uWS::App().get("/test", [](auto *res, auto *req) {
        res->beginWrite();
        res->write("First");
        res->write("Second");
        res->write("Third");
        res->end();
    }).listen(9001, [&listenSocket](auto *token) {
        if (token) {
            listenSocket = token;
            std::cout << "  Server started on port 9001" << std::endl;
            
            std::thread([&listenSocket]() {
                sleep(1);
                
                int sock = socket(AF_INET, SOCK_STREAM, 0);
                assert(sock >= 0);
                
                struct sockaddr_in addr;
                addr.sin_family = AF_INET;
                addr.sin_port = htons(9001);
                inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
                
                int result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
                assert(result == 0);
                
                const char *request = "GET /test HTTP/1.1\r\nHost: localhost\r\n\r\n";
                send(sock, request, strlen(request), 0);
                
                char buffer[4096] = {0};
                int bytesRead = recv(sock, buffer, sizeof(buffer) - 1, 0);
                assert(bytesRead > 0);
                
                std::string response(buffer, bytesRead);
                
                assert(response.find("Transfer-Encoding: chunked") != std::string::npos);
                assert(response.find("First") != std::string::npos);
                assert(response.find("Second") != std::string::npos);
                assert(response.find("Third") != std::string::npos);
                assert(response.find("\r\n0\r\n\r\n") != std::string::npos);
                
                close(sock);
                
                std::cout << "  ✓ beginWrite works correctly with chunked encoding" << std::endl;
                
                us_listen_socket_close(0, listenSocket);
            }).detach();
        } else {
            std::cerr << "Failed to listen on port 9001" << std::endl;
            exit(1);
        }
    }).run();
}

void testBeginWriteIdempotent() {
    std::cout << "TestBeginWriteIdempotent" << std::endl;
    
    us_listen_socket_t *listenSocket = nullptr;
    
    uWS::App().get("/idempotent", [](auto *res, auto *req) {
        res->beginWrite();
        res->beginWrite();
        res->write("Data1");
        res->beginWrite();
        res->write("Data2");
        res->end();
    }).listen(9002, [&listenSocket](auto *token) {
        if (token) {
            listenSocket = token;
            std::thread([&listenSocket]() {
                sleep(1);
                
                int sock = socket(AF_INET, SOCK_STREAM, 0);
                struct sockaddr_in addr;
                addr.sin_family = AF_INET;
                addr.sin_port = htons(9002);
                inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
                
                connect(sock, (struct sockaddr*)&addr, sizeof(addr));
                
                const char *request = "GET /idempotent HTTP/1.1\r\nHost: localhost\r\n\r\n";
                send(sock, request, strlen(request), 0);
                
                char buffer[4096] = {0};
                recv(sock, buffer, sizeof(buffer) - 1, 0);
                std::string response(buffer);
                
                assert(response.find("Transfer-Encoding: chunked") != std::string::npos);
                assert(response.find("Data1") != std::string::npos);
                assert(response.find("Data2") != std::string::npos);
                
                size_t firstPos = response.find("Transfer-Encoding: chunked");
                size_t secondPos = response.find("Transfer-Encoding: chunked", firstPos + 1);
                assert(secondPos == std::string::npos);
                
                close(sock);
                
                std::cout << "  ✓ beginWrite is idempotent (multiple calls safe)" << std::endl;
                
                us_listen_socket_close(0, listenSocket);
            }).detach();
        } else {
            exit(1);
        }
    }).run();
}

void testBeginWriteNoData() {
    std::cout << "TestBeginWriteNoData" << std::endl;
    
    us_listen_socket_t *listenSocket = nullptr;
    
    uWS::App().get("/nodata", [](auto *res, auto *req) {
        res->beginWrite();
        res->end();
    }).listen(9003, [&listenSocket](auto *token) {
        if (token) {
            listenSocket = token;
            std::thread([&listenSocket]() {
                sleep(1);
                
                int sock = socket(AF_INET, SOCK_STREAM, 0);
                struct sockaddr_in addr;
                addr.sin_family = AF_INET;
                addr.sin_port = htons(9003);
                inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
                
                connect(sock, (struct sockaddr*)&addr, sizeof(addr));
                
                const char *request = "GET /nodata HTTP/1.1\r\nHost: localhost\r\n\r\n";
                send(sock, request, strlen(request), 0);
                
                char buffer[4096] = {0};
                recv(sock, buffer, sizeof(buffer) - 1, 0);
                std::string response(buffer);
                
                assert(response.find("Transfer-Encoding: chunked") != std::string::npos);
                assert(response.find("\r\n0\r\n\r\n") != std::string::npos);
                
                close(sock);
                
                std::cout << "  ✓ beginWrite with no data works correctly" << std::endl;
                
                us_listen_socket_close(0, listenSocket);
            }).detach();
        } else {
            exit(1);
        }
    }).run();
}

int main() {
    std::cout << "Testing HttpResponse::beginWrite()" << std::endl;
    std::cout << std::endl;

    testBeginWrite();
    testBeginWriteIdempotent();
    testBeginWriteNoData();

    std::cout << std::endl;
    std::cout << "HTTP RESPONSE DONE" << std::endl;
    
    return 0;
}
