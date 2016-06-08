#ifndef UWS_CLIENT_H
#define UWS_CLIENT_H

#include <cstddef>
#include <functional>
#include <string>
#include <queue>
#include <mutex>

#include <uv.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <Windows.h>
#endif

namespace uWS_Client {

#ifdef _WIN32
typedef SOCKET FD;
#else
typedef int FD;
#endif

enum OpCode : unsigned char {
    TEXT = 1,
    BINARY = 2,
    CLOSE = 8,
    PING = 9,
    PONG = 10
};

enum Options : int {
    NO_OPTIONS = 0,
    PERMESSAGE_DEFLATE = 1,
    SERVER_NO_CONTEXT_TAKEOVER = 2,
    CLIENT_NO_CONTEXT_TAKEOVER = 4
};

struct Parser;
struct Request;

class Socket {
    friend class Client;
    friend struct Parser;
    friend struct std::hash<uWS_Client::Socket>;
protected:
    void *socket;
    Socket(void *p) : socket(p) {}
    void write(char *data, size_t length, bool transferOwnership, void(*callback)(void *s) = nullptr);
public:
    struct Address {
        unsigned int port;
        char *address;
        const char *family;
    };

    Address getAddress();
    void close(bool force = false, unsigned short code = 0, char *data = nullptr, size_t length = 0);
    void send(char *data, size_t length, OpCode opCode, size_t fakedLength = 0);
    void sendFragment(char *data, size_t length, OpCode opCode, size_t remainingBytes);
    void *getData();
    void setData(void *data);
    Socket() : socket(nullptr) {}
    bool operator==(const Socket &other) const {return socket == other.socket;}
    bool operator<(const Socket &other) const {return socket < other.socket;}
};

class Client
{
    friend struct Parser;
    friend class Socket;
private:
    // internal callbacks
    static void onReadable(void *vp, int status, int events);
    static void onWritable(void *vp, int status, int events);

    static void internalHTTP(Request &request);
    static void internalFragment(Socket socket, const char *fragment, size_t length, OpCode opCode, bool fin, size_t remainingBytes, bool compressed);

    // external callbacks
    std::function<void(Socket)> connectionSuccessCallback;
    std::function<void()> connectionFailureCallback;
    std::function<void(Socket, int code, char *message, size_t length)> disconnectionCallback;
    std::function<void(Socket, const char *, size_t, OpCode)> messageCallback;
    void (*fragmentCallback)(Socket, const char *, size_t, OpCode, bool, size_t, bool);

    // buffers
    char *receiveBuffer, *sendBuffer, *inflateBuffer;
    static const int BUFFER_SIZE = 307200,
                     SHORT_SEND = 4096;
    int maxPayload = 0;

    // accept poll
    void *client = nullptr;
    void *loop, *closeAsync;
    void *clients = nullptr;
    int port;
    bool forceClose, master;

    static void closeHandler(Client *client);
	const std::string host;
    int options;
    std::string path;
public:
	FD fd;
    Client(const std::string &host, int port, bool master = true, int options = 0, int maxPayload = 0);
    ~Client();
    Client(const Client &client) = delete;
    Client &operator=(const Client &client) = delete;
    void onConnectionSuccess(std::function<void(Socket)> connectionCallback);
    void onConnectionFailure(std::function<void()> connectionCallback);
    void onDisconnection(std::function<void(Socket, int code, char *message, size_t length)> disconnectionCallback);
    void onFragment(void (*fragmentCallback)(Socket, const char *, size_t, OpCode, bool, size_t, bool));
    void onMessage(std::function<void(Socket, const char *, size_t, OpCode)> messageCallback);
    void run();
	void connect();
    static bool isValidUtf8(unsigned char *str, size_t length);

    // thread safe (should have thread-unsafe counterparts)
    void close(bool force = false);
};

}

namespace std {
template <>
struct hash<uWS_Client::Socket> {
    std::size_t operator()(const uWS_Client::Socket &socket) const
    {
        return std::hash<void *>()(socket.socket);
    }
};
}

#endif // UWS_H
