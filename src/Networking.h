#ifndef NETWORKING_UWS_H
#define NETWORKING_UWS_H

#include <openssl/opensslv.h>
#if OPENSSL_VERSION_NUMBER < 0x10100000L
#define SSL_CTX_up_ref(x) x->references++
#define SSL_up_ref(x) x->references++
#endif

#ifndef __linux
#define MSG_NOSIGNAL 0
#else
#include <endian.h>
#endif

#ifdef __APPLE__
#define htobe64(x) OSSwapHostToBigInt64(x)
#define be64toh(x) OSSwapBigToHostInt64(x)
#endif

#ifdef _WIN32
#define NOMINMAX
#include <WinSock2.h>
#include <Ws2tcpip.h>
#define SHUT_WR SD_SEND
#define htobe64(x) htonll(x)
#define be64toh(x) ntohll(x)
#define __thread __declspec(thread)
#define pthread_t DWORD
#define pthread_self GetCurrentThreadId
#define WIN32_EXPORT __declspec(dllexport)

inline void close(SOCKET fd) {closesocket(fd);}
inline int setsockopt(SOCKET fd, int level, int optname, const void *optval, socklen_t optlen) {
    return setsockopt(fd, level, optname, (const char *) optval, optlen);
}

inline SOCKET dup(SOCKET socket) {
    WSAPROTOCOL_INFOW pi;
    if (WSADuplicateSocketW(socket, GetCurrentProcessId(), &pi) == SOCKET_ERROR) {
        return INVALID_SOCKET;
    }
    return WSASocketW(pi.iAddressFamily, pi.iSocketType, pi.iProtocol, &pi, 0, WSA_FLAG_OVERLAPPED);
}
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
#define WIN32_EXPORT
#endif

#include "uUV.h"
#include <openssl/ssl.h>
#include <vector>
#include <string>
#include <mutex>
#include <algorithm>
#include <memory>

namespace uS {

namespace TLS {

class WIN32_EXPORT Context {
private:
    SSL_CTX *context = nullptr;
    std::shared_ptr<std::string> password;

    static int passwordCallback(char *buf, int size, int rwflag, void *u)
    {
        std::string *password = (std::string *) u;
        int length = std::min<int>(size, password->length());
        memcpy(buf, password->data(), length);
        buf[length] = '\0';
        return length;
    }

public:
    friend Context createContext(std::string certChainFileName, std::string keyFileName, std::string keyFilePassword);
    Context(SSL_CTX *context) : context(context) {

    }

    Context() = default;
    Context(const Context &other);
    Context &operator=(const Context &other);
    ~Context();
    operator bool() {
        return context;
    }

    SSL_CTX *getNativeContext() {
        return context;
    }
};

Context createContext(std::string certChainFileName, std::string keyFileName, std::string keyFilePassword = std::string());

}

struct SocketData;

struct WIN32_EXPORT NodeData {
    char *recvBufferMemoryBlock;
    char *recvBuffer;
    int recvLength;
    uv_loop_t *loop;
    void *user = nullptr;
    static const int preAllocMaxSize = 1024;
    char **preAlloc;
    SSL_CTX *clientContext;

    uv_async_t *async = nullptr;
    pthread_t tid;

    struct TransferData {
        uv_poll_t *p;
        uv_os_sock_t fd;
        SocketData *socketData;
        uv_poll_cb pollCb;
        void (*cb)(uv_poll_t *);
    };

    void addAsync() {
        async = new uv_async_t;
        async->data = this;
        uv_async_init(loop, async, NodeData::asyncCallback);
    }

    std::mutex *asyncMutex;
    std::vector<TransferData> transferQueue;
    std::vector<uv_poll_t *> changePollQueue;
    static void asyncCallback(uv_async_t *async);

    static int getMemoryBlockIndex(size_t length) {
        return (length >> 4) + bool(length & 15);
    }

    char *getSmallMemoryBlock(int index) {
        if (preAlloc[index]) {
            char *memory = preAlloc[index];
            preAlloc[index] = nullptr;
            return memory;
        } else {
            return new char[index << 4];
        }
    }

    void freeSmallMemoryBlock(char *memory, int index) {
        if (!preAlloc[index]) {
            preAlloc[index] = memory;
        } else {
            delete [] memory;
        }
    }
};

struct SocketData {
    NodeData *nodeData;
    SSL *ssl;
    void *user = nullptr;

    // combine these two! state!
    int poll;
    bool shuttingDown = false;

    SocketData(NodeData *nodeData) : nodeData(nodeData) {

    }

    struct Queue {
        struct Message {
            const char *data;
            size_t length;
            Message *nextMessage = nullptr;
            void (*callback)(void *socket, void *data, bool cancelled, void *reserved) = nullptr;
            void *callbackData = nullptr, *reserved = nullptr;
        };

        Message *head = nullptr, *tail = nullptr;
        void pop()
        {
            Message *nextMessage;
            if ((nextMessage = head->nextMessage)) {
                delete [] (char *) head;
                head = nextMessage;
            } else {
                delete [] (char *) head;
                head = tail = nullptr;
            }
        }

        bool empty() {return head == nullptr;}
        Message *front() {return head;}

        void push(Message *message)
        {
            message->nextMessage = nullptr;
            if (tail) {
                tail->nextMessage = message;
                tail = message;
            } else {
                head = message;
                tail = message;
            }
        }
    } messageQueue;

    uv_poll_t *next = nullptr, *prev = nullptr;
};

struct ListenData : SocketData {

    ListenData(NodeData *nodeData) : SocketData(nodeData) {

    }

    uv_poll_t *listenPoll = nullptr;
    uv_timer_t *listenTimer = nullptr;
    uv_os_sock_t sock;
    uS::TLS::Context sslContext;
};

enum SocketState : unsigned char {
    CLOSED,
    POLL_READ,
    POLL_WRITE
};

}

#endif // NETWORKING_UWS_H
