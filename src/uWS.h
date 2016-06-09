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

struct Address {
	unsigned int port;
	char *address;
	const char *family;
};

struct Parser;
struct Request;
template <bool IsServer> class Agent;

template <bool IsServer>
class Socket {
    template <bool IsServer2> friend class Agent;
	friend class Server;
    friend struct Parser;
    friend struct std::hash<uWS::Socket<IsServer>>;
protected:
    void *socket;
    Socket(void *p) : socket(p) {}
    void write(char *data, size_t length, bool transferOwnership, void(*callback)(void *s) = nullptr);
public:
    Address getAddress();
    void close(bool force = false, unsigned short code = 0, char *data = nullptr, size_t length = 0);
    void send(char *data, size_t length, OpCode opCode, size_t fakedLength = 0);
    void sendFragment(char *data, size_t length, OpCode opCode, size_t remainingBytes);
    void *getData();
    void setData(void *data);
    Socket() : socket(nullptr) {}
    bool operator==(const Socket<IsServer> &other) const {return socket == other.socket;}
    bool operator<(const Socket<IsServer> &other) const {return socket < other.socket;}
};
template class Socket<true>;
template class Socket<false>;
typedef Socket<true> ServerSocket;
typedef Socket<false> ClientSocket;

template <bool IsServer>
class Agent
{
    friend struct Parser;
    template <bool IsServer2> friend class Socket;
	friend class Server;
protected:
    // internal callbacks
    static void onReadable(void *vp, int status, int events);
    static void onWritable(void *vp, int status, int events);

    static void internalHTTP(Request &request);
    static void internalFragment(Socket<IsServer> socket, const char *fragment, size_t length, OpCode opCode, bool fin, size_t remainingBytes, bool compressed);

    // external callbacks
    std::function<void(FD, const char *, void *, const char *, size_t)> upgradeCallback;
    std::function<void(Socket<IsServer>)> connectionCallback;
    std::function<void(Socket<IsServer>, int code, char *message, size_t length)> disconnectionCallback;
    std::function<void(Socket<IsServer>, const char *, size_t, OpCode)> messageCallback;
    void (*fragmentCallback)(Socket<IsServer>, const char *, size_t, OpCode, bool, size_t, bool);

    // buffers
    char *receiveBuffer, *sendBuffer, *inflateBuffer, *upgradeResponse;
    static const int BUFFER_SIZE = 307200,
                     SHORT_SEND = 4096;
    int maxPayload = 0;

    void *loop, *closeAsync;
    void *clients = nullptr;
    bool forceClose, master;
    static void closeHandler(Agent *agent);

public:
    Agent(bool master, int maxPayload = 0) : master(master), maxPayload(maxPayload) {};
    Agent(const Agent &server) = delete;
    Agent &operator=(const Agent &server) = delete;
    void onConnection(std::function<void(Socket<IsServer>)> connectionCallback);
    void onDisconnection(std::function<void(Socket<IsServer>, int code, char *message, size_t length)> disconnectionCallback);
    void onFragment(void (*fragmentCallback)(Socket<IsServer>, const char *, size_t, OpCode, bool, size_t, bool));
    void onMessage(std::function<void(Socket<IsServer>, const char *, size_t, OpCode)> messageCallback);
    void run();
    void broadcast(char *data, size_t length, OpCode opCode);
    static bool isValidUtf8(unsigned char *str, size_t length);
};
template class Agent<true>;
template class Agent<false>;

class Server : public Agent<true>
{
    friend struct Parser;
    friend class Socket<true>;
	friend class Agent<true>;
private:
    static void onAcceptable(void *vp, int status, int events);

	char *upgradeResponse;
	void *upgradeAsync;

    // accept poll
    void *server = nullptr;
    void *listenAddr;
    int port;

    // upgrade queue
    std::queue<std::tuple<FD, std::string, void *, std::string>> upgradeQueue;
    std::mutex upgradeQueueMutex;
    static void upgradeHandler(Server *server);
    int options;
    std::string path;

public:
    Server(int port = 0, bool master = true, int options = 0, int maxPayload = 0, std::string path = "/");
    ~Server();
    Server(const Server &server) = delete;
    Server &operator=(const Server &server) = delete;
    void onUpgrade(std::function<void(FD, const char *, void *, const char *, size_t)> upgradeCallback);
    void upgrade(FD fd, const char *secKey, void *ssl = nullptr, const char *extensions = nullptr, size_t extensionsLength = 0);

    // thread safe (should have thread-unsafe counterparts)
    void close(bool force = false);
};

}

namespace std {
template <>
struct hash<uWS::Socket<true>> {
    std::size_t operator()(const uWS::Socket<true> &socket) const
    {
        return std::hash<void *>()(socket.socket);
    }
};
template <>
struct hash<uWS::Socket<false>> {
    std::size_t operator()(const uWS::Socket<false> &socket) const
    {
        return std::hash<void *>()(socket.socket);
    }
};
}

#endif // UWS_H
