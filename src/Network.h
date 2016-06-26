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
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <Windows.h>
#define SHUT_WR SD_SEND
#define htobe64(x) htonll(x)
#define be64toh(x) ntohll(x)
#define __thread __declspec(thread)

inline void close(SOCKET fd) {closesocket(fd);}
inline const char *inet_ntop(int af, const void *src, char *dst, socklen_t size) {
    return InetNtop(af, (void *) src, dst, size);
}

inline SOCKET dup(SOCKET socket) {
    WSAPROTOCOL_INFO pi;
    WSADuplicateSocket(socket, GetCurrentProcessId(), &pi);
    return WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, &pi, 0, WSA_FLAG_OVERLAPPED);
}
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#endif

#endif // NETWORK_H
