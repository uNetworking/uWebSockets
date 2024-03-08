#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NUM_SOCKETS 10
uint64_t listen_socket_epoll_data = 0;
epoll_event ready_events[NUM_SOCKETS] = {};
static int accepted_sockets = 0;

int __wrap_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {

	// the listen socket
	if (fd == 500) {
		listen_socket_epoll_data = event->data.u64;
		return 0;
	} else {
		if (fd < 500) {

		} else {
			// on our FDs
			ready_events[fd - 500 - 1].data.u64 = event->data.u64;
			ready_events[fd - 500 - 1].events = EPOLLIN;
		}
		
		return 0;
	}
}

int __wrap_epoll_wait(int epfd, struct epoll_event *events,
               int maxevents, int timeout) {

	if (accepted_sockets != NUM_SOCKETS) {
		events[0].events = EPOLLIN;
		events[0].data.u64 = listen_socket_epoll_data;
		return 1;
	} else {
		for (int i = 0; i < NUM_SOCKETS; i++) {
			events[i] = ready_events[i];
		}
		return NUM_SOCKETS;
	}
}

int __wrap_recv(int sockfd, void *buf, size_t len, int flags) {
	const char request[] = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
	memcpy(buf, request, sizeof(request) - 1);
	return sizeof(request) - 1;
}

int __wrap_send(int sockfd, const void *buf, size_t len, int flags) {
	static int sent = 0;
	static clock_t lastTime = clock();
	if (++sent == 1000000) {
		// print how long it took to make 1 million requests
		clock_t newTime = clock();
		float elapsed = float(newTime - lastTime) / CLOCKS_PER_SEC;
		printf("Req/sec: %f\n", 1000000.0f / elapsed);
		sent = 0;
		lastTime = newTime;
	}

	return len;
}

int __wrap_bind() {
	return 0;
}

int __wrap_setsockopt() {
	return 0;
}

int __wrap_accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
	if (accepted_sockets < NUM_SOCKETS) {
		accepted_sockets++;
		return accepted_sockets + 500;
	} else {
		return -1;
	}
}

int __wrap_listen() {
	return 0;
}

int __wrap_socket(int domain, int type, int protocol) {
	return 500;
}

#ifdef __cplusplus
}
#endif
