/* This is a simple yet efficient WebSocket server benchmark much like WRK */

#include <libusockets.h>
//#include "helper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// request eller upgradeRequest samt webSocketFrame

unsigned char web_socket_request[26] = {130, 128 | 20, 1, 2, 3, 4};

char request[] = "GET / HTTP/1.1\r\n"
                 "Upgrade: websocket\r\n"
                 "Connection: Upgrade\r\n"
                 "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n"
                 "Host: server.example.com\r\n"
                 "Sec-WebSocket-Version: 13\r\n\r\n";
char *host;
int port;
int connections;

int responses;

struct http_socket {
    /* How far we have streamed our request */
    int offset;
};

/* We don't need any of these */
void on_wakeup(struct us_loop *loop) {

}

void on_pre(struct us_loop *loop) {

}

/* This is not HTTP POST, it is merely an event emitted post loop iteration */
void on_post(struct us_loop *loop) {

}

struct us_socket *on_http_socket_writable(struct us_socket *s) {
    struct http_socket *http_socket = (struct http_socket *) us_socket_ext(s);

    /* Stream whatever is remaining of the request */
    http_socket->offset += us_socket_write(s, web_socket_request + http_socket->offset, sizeof(web_socket_request) - http_socket->offset, 0);

    return s;
}

struct us_socket *on_http_socket_close(struct us_socket *s) {

    printf("Closed!\n");

    return s;
}

struct us_socket *on_http_socket_end(struct us_socket *s) {
    return us_socket_close(s);
}

struct us_socket *on_http_socket_data(struct us_socket *s, char *data, int length) {
    /* Get socket extension and the socket's context's extension */
    struct http_socket *http_socket = (struct http_socket *) us_socket_ext(s);
    struct http_context *http_context = (struct http_context *) us_socket_context_ext(us_socket_get_context(s));

    /* We treat all data events as a response */
    http_socket->offset = us_socket_write(s, web_socket_request, sizeof(web_socket_request), 0);

    /* */
    responses++;

    //printf("Fick ett svar\n");

    return s;
}

struct us_socket *on_http_socket_open(struct us_socket *s, int is_client) {
    struct http_socket *http_socket = (struct http_socket *) us_socket_ext(s);

    /* Reset offset */
    http_socket->offset = 0;

    /* Send a request */
    us_socket_write(s, request, sizeof(request) - 1, 0);

    if (--connections) {
        us_socket_context_connect(us_socket_get_context(s), host, port, 0, sizeof(struct http_socket));
    } else {
        printf("Running benchmark now...\n");

        us_socket_timeout(s, LIBUS_TIMEOUT_GRANULARITY);
    }

    return s;
}

struct us_socket *on_http_socket_timeout(struct us_socket *s) {
    /* Print current statistics */
    printf("Req/sec: %f\n", ((float)responses) / LIBUS_TIMEOUT_GRANULARITY);

    responses = 0;
    us_socket_timeout(s, LIBUS_TIMEOUT_GRANULARITY);

    return s;
}

int main(int argc, char **argv) {

    /* Parse host and port */
    if (argc != 4) {
        printf("Usage: connections host port\n");
        return 0;
    }

    port = atoi(argv[3]);
    host = malloc(strlen(argv[2]) + 1);
    memcpy(host, argv[2], strlen(argv[2]) + 1);
    connections = atoi(argv[1]);

    /* Create the event loop */
    struct us_loop *loop = us_create_loop(1, on_wakeup, on_pre, on_post, 0);

    /* Create a socket context for HTTP */
#ifndef LIBUS_NO_SSL
    struct us_ssl_socket_context_options ssl_options = {};
    struct us_socket_context *http_context = us_create_ssl_socket_context(loop, 0, ssl_options);
#else
    struct us_socket_context *http_context = us_create_socket_context(loop, 0);
#endif

    /* Set up event handlers */
    us_socket_context_on_open(http_context, on_http_socket_open);
    us_socket_context_on_data(http_context, on_http_socket_data);
    us_socket_context_on_writable(http_context, on_http_socket_writable);
    us_socket_context_on_close(http_context, on_http_socket_close);
    us_socket_context_on_timeout(http_context, on_http_socket_timeout);
    us_socket_context_on_end(http_context, on_http_socket_end);

    /* Start making HTTP connections */
    us_socket_context_connect(http_context, host, port, 0, sizeof(struct http_socket));

    us_loop_run(loop);
}
