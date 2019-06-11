/* uSockets is entierly opaque so we can use the real header straight up */
#include "../uSockets/src/libusockets.h"

#include <stdio.h>
#include <stdlib.h>

struct us_loop_t {

    /* We only support one listen socket */
    struct us_listen_socket_t *listen_socket;
};

struct us_loop_t *us_create_loop(void *hint, void (*wakeup_cb)(struct us_loop_t *loop), void (*pre_cb)(struct us_loop_t *loop), void (*post_cb)(struct us_loop_t *loop), unsigned int ext_size) {
    return (struct us_loop_t *) malloc(sizeof(struct us_loop_t) + ext_size);
}

void us_loop_free(struct us_loop_t *loop) {
    free(loop);
}

void *us_loop_ext(struct us_loop_t *loop) {
    return loop + 1;
}

void us_loop_run(struct us_loop_t *loop) {

}

struct us_socket_context_t {
    struct us_loop_t *loop;

    struct us_socket_t *(*on_open)(struct us_socket_t *s, int is_client, char *ip, int ip_length);
    struct us_socket_t *(*on_close)(struct us_socket_t *s);
    struct us_socket_t *(*on_data)(struct us_socket_t *s, char *data, int length);
    struct us_socket_t *(*on_writable)(struct us_socket_t *s);
    struct us_socket_t *(*on_timeout)(struct us_socket_t *s);
    struct us_socket_t *(*on_end)(struct us_socket_t *s);
};

struct us_socket_context_t *us_create_socket_context(int ssl, struct us_loop_t *loop, int ext_size, struct us_socket_context_options_t options) {
    struct us_socket_context_t *socket_context = (struct us_socket_context_t *) malloc(sizeof(struct us_socket_context_t) + ext_size);

    socket_context->loop = loop;

    return socket_context;
}

void us_socket_context_free(int ssl, struct us_socket_context_t *context) {
    free(context);
}

void us_socket_context_on_open(int ssl, struct us_socket_context_t *context, struct us_socket_t *(*on_open)(struct us_socket_t *s, int is_client, char *ip, int ip_length)) {
    context->on_open = on_open;
}

void us_socket_context_on_close(int ssl, struct us_socket_context_t *context, struct us_socket_t *(*on_close)(struct us_socket_t *s)) {
    context->on_close = on_close;
}

void us_socket_context_on_data(int ssl, struct us_socket_context_t *context, struct us_socket_t *(*on_data)(struct us_socket_t *s, char *data, int length)) {
    context->on_data = on_data;
}

void us_socket_context_on_writable(int ssl, struct us_socket_context_t *context, struct us_socket_t *(*on_writable)(struct us_socket_t *s)) {
    context->on_writable = on_writable;
}

void us_socket_context_on_timeout(int ssl, struct us_socket_context_t *context, struct us_socket_t *(*on_timeout)(struct us_socket_t *s)) {
    context->on_timeout = on_timeout;
}

void us_socket_context_on_end(int ssl, struct us_socket_context_t *context, struct us_socket_t *(*on_end)(struct us_socket_t *s)) {
    context->on_end = on_end;
}

void *us_socket_context_ext(int ssl, struct us_socket_context_t *context) {
    return context + 1;
}

struct us_listen_socket_t {
    int socket_ext_size;
    struct us_socket_context_t *context;
};

struct us_listen_socket_t *us_socket_context_listen(int ssl, struct us_socket_context_t *context, const char *host, int port, int options, int socket_ext_size) {
    struct us_listen_socket_t *listen_socket = (struct us_listen_socket_t *) malloc(sizeof(struct us_listen_socket_t));

    listen_socket->socket_ext_size = socket_ext_size;
    listen_socket->context = context;

    context->loop->listen_socket = listen_socket;

    return listen_socket;
}

void us_listen_socket_close(int ssl, struct us_listen_socket_t *ls) {
    free(ls);
}

struct us_socket_t {
    struct us_socket_context_t *context;
};

/* For ubsan? */
struct us_new_socket_t {
    struct us_socket_context_t *context;
};

struct us_socket_t *us_socket_context_connect(int ssl, struct us_socket_context_t *context, const char *host, int port, int options, int socket_ext_size) {
    printf("us_socket_context_connect\n");
}

struct us_loop_t *us_socket_context_loop(int ssl, struct us_socket_context_t *context) {
    return context->loop;
}

struct us_socket_t *us_socket_context_adopt_socket(int ssl, struct us_socket_context_t *context, struct us_socket_t *s, int ext_size) {
    printf("us_socket_context_adopt_socket\n");
}

struct us_socket_context_t *us_create_child_socket_context(int ssl, struct us_socket_context_t *context, int context_ext_size) {
    printf("us_create_child_socket_context\n");
}

int us_socket_write(int ssl, struct us_socket_t *s, const char *data, int length, int msg_more) {

}

void us_socket_timeout(int ssl, struct us_socket_t *s, unsigned int seconds) {

}

void *us_socket_ext(int ssl, struct us_socket_t *s) {
    return s + 1;
}

struct us_socket_context_t *us_socket_context(int ssl, struct us_socket_t *s) {
    return s->context;
}

void us_socket_flush(int ssl, struct us_socket_t *s) {

}

void us_socket_shutdown(int ssl, struct us_socket_t *s) {

}

int us_socket_is_shut_down(int ssl, struct us_socket_t *s) {
    return 0;
}

int us_socket_is_closed(int ssl, struct us_socket_t *s) {
    return 0;
}

struct us_socket_t *us_socket_close(int ssl, struct us_socket_t *s) {
    return s;
}

void us_socket_remote_address(int ssl, struct us_socket_t *s, char *buf, int *length) {
    printf("us_socket_remote_address\n");
}

/* We expose this function to let fuzz targets push data to uSockets */
void us_loop_read_mocked_data(struct us_loop_t *loop, char *data, unsigned int size) {

    /* We have one listen socket */
    int socket_ext_size = loop->listen_socket->socket_ext_size;

    /* Create a socket with information from the listen socket */
    struct us_socket_t *s = (struct us_socket_t *) malloc(sizeof(struct us_socket_t) + socket_ext_size);
    s->context = loop->listen_socket->context;

    /* Emit open event */
    s->context->on_open(s, 0, 0, 0);

    /* Emit a bunch of data events here */
    s->context->on_data(s, data, size);

    /* Emit close event */
    s->context->on_close(s);

    /* Free the socket */
    free(s);
}