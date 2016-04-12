#ifndef UWS_H
#define UWS_H

#if defined(_WIN32) && defined(__cplusplus_cli)
#define HAVE_WIN32_CPP_CLI 1
#pragma managed(push, off)
#else
#ifndef HAVE_WIN32_CPP_CLI 
#define HAVE_WIN32_CPP_CLI 0
#endif
#endif

#ifndef USE_WIN32_THREADS
#if HAVE_WIN32_CPP_CLI
#define USE_WIN32_THREADS 1
#else
#define USE_WIN32_THREADS 0
#endif
#endif

#include <cstddef>
#include <functional>
#include <queue>

#if !USE_WIN32_THREADS
#include <mutex>
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <Windows.h>
#endif

#include <uv.h>

namespace uWS {

class MutexImpl {
private:
#if USE_WIN32_THREADS
  CRITICAL_SECTION m_mutex;
#else
  std::mutex m_mutex;
#endif
  
  MutexImpl(const MutexImpl& other) = delete;
  MutexImpl& operator=(const MutexImpl& other) = delete;

public:
  MutexImpl() {
#if USE_WIN32_THREADS
    InitializeCriticalSection(&m_mutex);
#endif
  }

  ~MutexImpl() {
#if USE_WIN32_THREADS
    DeleteCriticalSection(&m_mutex);
#endif
  }

  void lock() {
#if USE_WIN32_THREADS
    EnterCriticalSection(&m_mutex);
#else
    m_mutex.lock();
#endif
  }

  void unlock() {
#if USE_WIN32_THREADS
    LeaveCriticalSection(&m_mutex);
#else
    m_mutex.unlock();
#endif
  }
};

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
    MutexImpl upgradeQueueMutex;
    static void upgradeHandler(Server *server);

public:
    // thread unsafe
    Server(int port, bool deafultLoop = false);
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

#if HAVE_WIN32_CPP_CLI
#pragma managed(pop)
#endif

#endif // UWS_H
