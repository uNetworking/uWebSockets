/* This is a scalability test for testing million(s) of pinging connections */

#include <libusockets.h>
int SSL;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* Send ping every 16 seconds */
int WEBSOCKET_PING_INTERVAL = 16;

/* We only establish 20k connections per address */
int CONNECTIONS_PER_ADDRESS = 20000;

/* How many connections a time */
int BATCH_CONNECT = 1;

/* Currently open and alive connections */
int opened_connections;
/* Dead connections */
int closed_connections;

struct http_socket {
    /* How far we have streamed our websocket request */
    int offset;

    /* How far we have streamed our upgrade request */
    int upgrade_offset;
};

/* We don't need any of these */
void on_wakeup(struct us_loop_t *loop) {

}

void on_pre(struct us_loop_t *loop) {

}

/* This is not HTTP POST, it is merely an event emitted post loop iteration */
void on_post(struct us_loop_t *loop) {

}

void next_connection(struct us_socket_t *s) {
    /* We could wait with this until properly upgraded */
    if (--connections/* > BATCH_CONNECT*/) {
        /* Swap address */
        int address = opened_connections / CONNECTIONS_PER_ADDRESS + 1;
        char buf[16];
        sprintf(buf, "127.0.0.%d", address);

        us_socket_context_connect(SSL, us_socket_context(SSL, s), buf, port, 0, sizeof(struct http_socket));
    }
}

struct us_socket_t *on_http_socket_writable(struct us_socket_t *s) {
    struct http_socket *http_socket = (struct http_socket *) us_socket_ext(SSL, s);

    /* Are we still not upgraded yet? */
    if (http_socket->upgrade_offset < sizeof(request) - 1) {
        http_socket->upgrade_offset += us_socket_write(SSL, s, request + http_socket->upgrade_offset, sizeof(request) - 1 - http_socket->upgrade_offset, 0);

        /* Now we should be */
        if (http_socket->upgrade_offset == sizeof(request) - 1) {
            next_connection(s);

            /* Make sure to send ping */
            us_socket_timeout(SSL, s, WEBSOCKET_PING_INTERVAL);
        }
    } else {
        /* Stream whatever is remaining of the request */
        http_socket->offset += us_socket_write(SSL, s, (char *) web_socket_request + http_socket->offset, sizeof(web_socket_request) - http_socket->offset, 0);
        if (http_socket->offset == sizeof(web_socket_request)) {
            /* Reset timeout if we managed to */
            us_socket_timeout(SSL, s, WEBSOCKET_PING_INTERVAL);
        }
    }

    return s;
}

struct us_socket_t *on_http_socket_close(struct us_socket_t *s) {

    closed_connections++;
    if (closed_connections % 1000 == 0) {
        printf("Alive: %d, dead: %d\n", opened_connections, closed_connections);
    }

    return s;
}

struct us_socket_t *on_http_socket_end(struct us_socket_t *s) {
    return us_socket_close(SSL, s);
}

// should never get a response!
struct us_socket_t *on_http_socket_data(struct us_socket_t *s, char *data, int length) {
    return s;
}

struct us_socket_t *on_http_socket_open(struct us_socket_t *s, int is_client, char *ip, int ip_length) {
    struct http_socket *http_socket = (struct http_socket *) us_socket_ext(SSL, s);

    /* Display number of opened connections */
    opened_connections++;
    if (opened_connections % 1000 == 0) {
        printf("Alive: %d, dead: %d\n", opened_connections, closed_connections);
    }

    /* Send an upgrade request */
    http_socket->upgrade_offset = us_socket_write(SSL, s, request, sizeof(request) - 1, 0);
    if (http_socket->upgrade_offset == sizeof(request) - 1) {
        next_connection(s);

        /* Make sure to send ping */
        us_socket_timeout(SSL, s, WEBSOCKET_PING_INTERVAL);
    }

    return s;
}

// here we should send a message as ping (part of the test)
struct us_socket_t *on_http_socket_timeout(struct us_socket_t *s) {
    struct http_socket *http_socket = (struct http_socket *) us_socket_ext(SSL, s);

    /* Send ping here */
    http_socket->offset = us_socket_write(SSL, s, (char *) web_socket_request, sizeof(web_socket_request), 0);
    if (http_socket->offset == sizeof(web_socket_request)) {
        /* Reset timeout if we managed to */
        us_socket_timeout(SSL, s, WEBSOCKET_PING_INTERVAL);
    }

    return s;
}

int main(int argc, char **argv) {

    /* Parse host and port */
    if (argc != 5) {
        printf("Usage: connections host port ssl\n");
        return 0;
    }

    port = atoi(argv[3]);
    host = malloc(strlen(argv[2]) + 1);
    memcpy(host, argv[2], strlen(argv[2]) + 1);
    connections = atoi(argv[1]);
    SSL = atoi(argv[4]);

    /* Create the event loop */
    struct us_loop_t *loop = us_create_loop(0, on_wakeup, on_pre, on_post, 0);

    /* Create a socket context for HTTP */
    struct us_socket_context_options_t options = {};
    struct us_socket_context_t *http_context = us_create_socket_context(SSL, loop, 0, options);

    /* Set up event handlers */
    us_socket_context_on_open(SSL, http_context, on_http_socket_open);
    us_socket_context_on_data(SSL, http_context, on_http_socket_data);
    us_socket_context_on_writable(SSL, http_context, on_http_socket_writable);
    us_socket_context_on_close(SSL, http_context, on_http_socket_close);
    us_socket_context_on_timeout(SSL, http_context, on_http_socket_timeout);
    us_socket_context_on_end(SSL, http_context, on_http_socket_end);

    /* Start making HTTP connections */
    for (int i = 0; i < BATCH_CONNECT; i++) {
        us_socket_context_connect(SSL, http_context, host, port, 0, sizeof(struct http_socket));
    }

    us_loop_run(loop);
}
