#ifndef UWS_H
#define UWS_H

#include <cstddef>
#include <functional>
#include <string>
#include <queue>
#include <mutex>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <Windows.h>
#endif

namespace uWS {

#ifdef _WIN32
typedef SOCKET FD;
#else
typedef int FD;
#endif

enum OpCode : unsigned char {
    TEXT = 1,
    BINARY = 2,
    PING = 9,
    PONG = 10
};

struct Parser;
struct Request;

class Socket {
    friend class Server;
    friend struct Parser;
protected:
    void *socket;
    Socket(void *p) : socket(p) {}
    void write(char *data, size_t length, bool transferOwnership, void(*callback)(FD fd) = nullptr);
public:
    void close(bool force = false);
    void send(char *data, size_t length, OpCode opCode, size_t fakedLength = 0);
    void sendFragment(char *data, size_t length, OpCode opCode, size_t remainingBytes);
    void *getData();
    void setData(void *data);
};

class Server
{
    friend struct Parser;
    friend class Socket;
private:
    // internal callbacks
    static void onReadable(void *vp, int status, int events);
    static void onWritable(void *vp, int status, int events);
    static void onAcceptable(void *vp, int status, int events);

    static void internalHTTP(Request &request);
    static void internalFragment(Socket socket, const char *fragment, size_t length, OpCode opCode, bool fin, size_t remainingBytes);

    // external callbacks
    std::function<void(FD, const char *)> upgradeCallback;
    std::function<void(Socket)> connectionCallback, disconnectionCallback;
    std::function<void(Socket, const char *, size_t, OpCode)> messageCallback;
    void (*fragmentCallback)(Socket, const char *, size_t, OpCode, bool, size_t);

    // buffers
    char *receiveBuffer, *sendBuffer;
    static const int BUFFER_SIZE = 307200,
                     SHORT_SEND = 4096;

    // accept poll
    void *server = nullptr;
    void *listenAddr;
    void *loop, *timer, *upgradeAsync, *closeAsync;
    void *clients = nullptr;
    bool forceClose;
    int port = 0;
    bool defaultLoop;

    // upgrade queue
    std::queue<std::pair<FD, std::string>> upgradeQueue;
    std::mutex upgradeQueueMutex;
    static void upgradeHandler(Server *server);
    std::string path;

public:
    // thread unsafe
    Server(int port, bool deafultLoop = false, std::string path = "/");
    ~Server();
    Server(const Server &server) = delete;
    Server &operator=(const Server &server) = delete;
    void onUpgrade(std::function<void(FD, const char *)> upgradeCallback);
    void onConnection(std::function<void(Socket)> connectionCallback);
    void onDisconnection(std::function<void(Socket)> disconnectionCallback);
    void onFragment(void (*fragmentCallback)(Socket, const char *, size_t, OpCode, bool, size_t));
    void onMessage(std::function<void(Socket, const char *, size_t, OpCode)> messageCallback);
    void run();
    void broadcast(char *data, size_t length, OpCode opCode);
    static bool isValidUtf8(unsigned char *str, size_t length);

    // thread safe (should have thread-unsafe counterparts)
    void close(bool force = false);
    void upgrade(FD fd, const char *secKey, bool dupFd = false, bool immediately = false);
};

}

#endif // UWS_H
