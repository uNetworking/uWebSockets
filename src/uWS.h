#ifndef UWS_H
#define UWS_H

#include <cstddef>
#include <functional>
#include <string>
#include <queue>
#include <mutex>

namespace uWS {

#ifdef _WIN32
typedef HANDLE FD;
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
private:
    void *socket;
    Socket(void *p) : socket(p) {}
    void write(char *data, size_t length, bool transferOwnership);
public:
    void close(bool force = false);
    void send(char *data, size_t length, OpCode opCode, size_t fakedLength = 0);
    void sendFragment(char *data, size_t length, OpCode opCode, size_t remainingBytes);
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
    void (*upgradeCallback)(FD, const char *) = nullptr;
    void (*connectionCallback)(Socket) = nullptr;
    void (*disconnectionCallback)(Socket) = nullptr;
    void (*fragmentCallback)(Socket, const char *, size_t, OpCode, bool, size_t) = nullptr;
    void (*messageCallback)(Socket socket, const char *message, size_t length, OpCode opCode) = nullptr;

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

    // upgrade queue
    std::queue<std::pair<FD, std::string>> upgradeQueue;
    std::mutex upgradeQueueMutex;

public:
    // thread unsafe
    Server(int port);
    ~Server();
    Server(const Server &server) = delete;
    Server &operator=(const Server &server) = delete;
    void onUpgrade(void (*upgradeCallback)(FD, const char *));
    void onConnection(void (*connectionCallback)(Socket));
    void onDisconnection(void (*disconnectionCallback)(Socket));
    void onFragment(void (*fragmentCallback)(Socket, const char *, size_t, OpCode, bool, size_t));
    void onMessage(void (*messageCallback)(Socket, const char *, size_t, OpCode));
    void run();
    void broadcast(char *data, size_t length, OpCode opCode);
    static bool isValidUtf8(unsigned char *str, size_t length);

    // thread safe (should have thread-unsafe counterparts)
    void close(bool force = false);
    void upgrade(FD fd, const char *secKey);
};

}

#endif // UWS_H
