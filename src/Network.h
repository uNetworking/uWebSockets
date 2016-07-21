#ifndef NETWORK_H
#define NETWORK_H

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
#endif

#endif // NETWORK_H
