/* This is a simple yet efficient WebSocket server benchmark much like WRK */

#include <libusockets.h>
int SSL;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// request eller upgradeRequest samt webSocketFrame

/* Whatever type we selected (compressed or not) */
unsigned char *web_socket_request;
int web_socket_request_size;

char *upgrade_request;
int upgrade_request_length;

/* Compressed message */
unsigned char web_socket_request_deflate[13] = {
    130 | 64, 128 | 7,
    0, 0, 0, 0,
    0xf2, 0x48, 0xcd, 0xc9, 0xc9, 0x07, 0x00
};

/* Not compressed */
unsigned char web_socket_request_text[26] = {130, 128 | 20, 1, 2, 3, 4};


char request_deflate[] = "GET / HTTP/1.1\r\n"
                 "Upgrade: websocket\r\n"
                 "Connection: Upgrade\r\n"
                 "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n"
                 "Sec-WebSocket-Extensions: permessage-deflate; client_max_window_bits\r\n"
                 "Host: server.example.com\r\n"
                 "Sec-WebSocket-Version: 13\r\n\r\n";

char request_text[] = "GET / HTTP/1.1\r\n"
                 "Upgrade: websocket\r\n"
                 "Connection: Upgrade\r\n"
                 "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n"
                 //"Sec-WebSocket-Extensions: permessage-deflate; client_max_window_bits\r\n"
                 "Host: server.example.com\r\n"
                 "Sec-WebSocket-Version: 13\r\n\r\n";
char *host;
int port;
int connections;

int responses;

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
    if (--connections) {
        us_socket_context_connect(SSL, us_socket_context(SSL, s), host, port, NULL, 0, sizeof(struct http_socket));
    } else {
        printf("Running benchmark now...\n");

        us_socket_timeout(SSL, s, LIBUS_TIMEOUT_GRANULARITY);
    }
}

struct us_socket_t *on_http_socket_writable(struct us_socket_t *s) {
    struct http_socket *http_socket = (struct http_socket *) us_socket_ext(SSL, s);

    /* Are we still not upgraded yet? */
    if (http_socket->upgrade_offset < upgrade_request_length) {
        http_socket->upgrade_offset += us_socket_write(SSL, s, upgrade_request + http_socket->upgrade_offset, upgrade_request_length - http_socket->upgrade_offset, 0);

        /* Now we should be */
        if (http_socket->upgrade_offset == upgrade_request_length) {
            next_connection(s);
        }
    } else {
        /* Stream whatever is remaining of the request */
        http_socket->offset += us_socket_write(SSL, s, (char *) web_socket_request + http_socket->offset, web_socket_request_size - http_socket->offset, 0);
    }

    return s;
}

struct us_socket_t *on_http_socket_close(struct us_socket_t *s, int code, void *reason) {

    printf("Closed!\n");

    return s;
}

struct us_socket_t *on_http_socket_end(struct us_socket_t *s) {
    return us_socket_close(SSL, s, 0, NULL);
}

struct us_socket_t *on_http_socket_data(struct us_socket_t *s, char *data, int length) {
    /* Get socket extension and the socket's context's extension */
    struct http_socket *http_socket = (struct http_socket *) us_socket_ext(SSL, s);
    //struct http_context *http_context = (struct http_context *) us_socket_context_ext(SSL, us_socket_context(SSL, s));

    /* We treat all data events as a response */
    http_socket->offset = us_socket_write(SSL, s, (char *) web_socket_request, web_socket_request_size, 0);

    /* */
    responses++;

    return s;
}

struct us_socket_t *on_http_socket_open(struct us_socket_t *s, int is_client, char *ip, int ip_length) {
    struct http_socket *http_socket = (struct http_socket *) us_socket_ext(SSL, s);

    /* Reset offsets */
    http_socket->offset = 0;

    /* Send an upgrade request */
    http_socket->upgrade_offset = us_socket_write(SSL, s, upgrade_request, upgrade_request_length, 0);
    if (http_socket->upgrade_offset == upgrade_request_length) {
        next_connection(s);
    }

    return s;
}

struct us_socket_t *on_http_socket_timeout(struct us_socket_t *s) {
    /* Print current statistics */
    printf("Msg/sec: %f\n", ((float)responses) / LIBUS_TIMEOUT_GRANULARITY);

    responses = 0;
    us_socket_timeout(SSL, s, LIBUS_TIMEOUT_GRANULARITY);

    return s;
}

int main(int argc, char **argv) {

    /* Parse host and port */
    if (argc != 6) {
        printf("Usage: connections host port ssl deflate\n");
        return 0;
    }

    port = atoi(argv[3]);
    host = malloc(strlen(argv[2]) + 1);
    memcpy(host, argv[2], strlen(argv[2]) + 1);
    connections = atoi(argv[1]);
    SSL = atoi(argv[4]);
    if (atoi(argv[5])) {
        /* Set up deflate */
        web_socket_request = web_socket_request_deflate;
        web_socket_request_size = sizeof(web_socket_request_deflate);

        upgrade_request = request_deflate;
        upgrade_request_length = sizeof(request_deflate) - 1;
    } else {
        web_socket_request = web_socket_request_text;
        web_socket_request_size = sizeof(web_socket_request_text);

        upgrade_request = request_text;
        upgrade_request_length = sizeof(request_text) - 1;
    }

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
    us_socket_context_connect(SSL, http_context, host, port, NULL, 0, sizeof(struct http_socket));

    us_loop_run(loop);
}
