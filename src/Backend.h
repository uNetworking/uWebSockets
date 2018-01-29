#ifndef BACKEND_H
#define BACKEND_H

// Default to Epoll if nothing specified and on Linux
// Default to Libuv if nothing specified and not on Linux
#ifdef USE_ASIO
#include "Asio.h"
#elif !defined(__linux__) || defined(USE_LIBUV)
#include "Libuv.h"
#else
#ifndef USE_EPOLL
#define USE_EPOLL
#endif
#include "Epoll.h"
#endif

#endif // BACKEND_H
