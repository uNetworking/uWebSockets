#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <string>
#include <chrono>
#include <errno.h>
#include <vector>
#include <mutex>
#include <thread>
#include <fstream>
using namespace std;
using namespace chrono;

int totalConnections = 500000;
int port = 3000;

#define CONNECTIONS_PER_ADDRESS 28000
#define THREADS 10

int connections, address = 1;
mutex m;

int getKb(int pid) {
    std::string line;
    std::ifstream self((std::string("/proc/") + std::to_string(pid) + std::string("/status")).c_str());
    int vmRSS;
    while(!self.eof()) {
        std::getline(self, line, ':');
        if (line == "VmRSS") {
            self >> vmRSS;
        }
        std::getline(self, line);
    }
    return vmRSS;
}

bool nextConnection(int tid)
{
    m.lock();
    int socketfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socketfd == -1) {
        cout << "FD error, connections: " << connections << endl;
        return false;
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(("127.0.0." + to_string(address)).c_str());
    addr.sin_port = htons(port);
    m.unlock();

    // this is a shared upgrade, no need to make it unique
    const char *buf = "GET / HTTP/1.1\r\n"
                      "Host: server.example.com\r\n"
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n"
                      "Sec-WebSocket-Protocol: default\r\n"
                      "Sec-WebSocket-Version: 13\r\n"
                      "Origin: http://example.com\r\n\r\n";

    char message[1024];

    int err = connect(socketfd, (sockaddr *) &addr, sizeof(addr));
    if (err) {
        cout << "Connection error, connections: " << connections << endl;
        return false;
    }
    send(socketfd, buf, strlen(buf), 0);
    memset(message, 0, 1024);
    size_t length;
    do {
        length = recv(socketfd, message, sizeof(message), 0);
    }
    while (strncmp(&message[length - 4], "\r\n\r\n", 4));

    m.lock();
    if (++connections % CONNECTIONS_PER_ADDRESS == 0) {
        address++;
    }

    if (connections % 1000 == 0 || connections < 1000) {
        cout << "Connections: " << connections << endl;
    }

    if (connections >= totalConnections - THREADS + 1) {
        m.unlock();
        return false;
    }

    m.unlock();
    return true;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        cout << "Usage: scalability numberOfConnections port" << endl;
        return -1;
    }

    totalConnections = atoi(argv[1]);
    port = atoi(argv[2]);

    FILE *pipe = popen(("fuser " + to_string(port) + "/tcp 2> /dev/null").c_str(), "r");
    char line[10240] = {};
    fgets(line, sizeof(line), pipe);
    pclose(pipe);
    int pid = atoi(line);

    auto startPoint = high_resolution_clock::now();
    vector<thread *> threads;
    for (int i = 0; i < THREADS; i++) {
        threads.push_back(new thread([i] {
            while(nextConnection(i));
        }));
    }

    for (thread *t : threads) {
        t->join();
    }

    double connectionsPerMs = double(connections) / duration_cast<milliseconds>(high_resolution_clock::now() - startPoint).count();
    cout << "Connection performance: " << connectionsPerMs << " connections/ms" << endl;

    unsigned long kbUsage = getKb(pid);
    cout << "Memory usage: " << (double(kbUsage) / 1024.0) << " mb of user space" << std::endl;
    cout << "Memory performance: " << 1024.0 * double(connections) / kbUsage << " connections/mb" << endl;
    return 0;
}

