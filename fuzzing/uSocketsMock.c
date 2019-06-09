/* uSockets is entierly opaque so we can use the real header straight up */
#include "../uSockets/src/libusockets_new.h"

#include <stdio.h>
#include <stdlib.h>

struct us_loop {

    /* We only support one listen socket */
    struct us_listen_socket *listen_socket;
};

struct us_loop *us_create_loop(int default_hint, void (*wakeup_cb)(struct us_loop *loop), void (*pre_cb)(struct us_loop *loop), void (*post_cb)(struct us_loop *loop), unsigned int ext_size) {
    return (struct us_loop *) malloc(sizeof(us_loop) + ext_size);
}

void us_loop_free(struct us_loop *loop) {
    free(loop);
}

void *us_loop_ext(struct us_loop *loop) {
    return loop + 1;
}

void us_loop_run(struct us_loop *loop) {

}

struct us_socket_context {
    struct us_loop *loop;

    struct us_socket *(*on_open)(struct us_socket *s, int is_client, char *ip, int ip_length);
    struct us_socket *(*on_close)(struct us_socket *s);
    struct us_socket *(*on_data)(struct us_socket *s, char *data, int length);
    struct us_socket *(*on_writable)(struct us_socket *s);
    struct us_socket *(*on_timeout)(struct us_socket *s);
    struct us_socket *(*on_end)(struct us_socket *s);
};

struct us_socket_context *us_create_socket_context(struct us_loop *loop, int ext_size) {
    struct us_socket_context *socket_context = (struct us_socket_context *) malloc(sizeof(us_socket_context) + ext_size);

    socket_context->loop = loop;

    return socket_context;
}

void us_socket_context_free(struct us_socket_context *context) {
    free(context);
}

void us_socket_context_on_open(struct us_socket_context *context, struct us_socket *(*on_open)(struct us_socket *s, int is_client, char *ip, int ip_length)) {
    context->on_open = on_open;
}

void us_socket_context_on_close(struct us_socket_context *context, struct us_socket *(*on_close)(struct us_socket *s)) {
    context->on_close = on_close;
}

void us_socket_context_on_data(struct us_socket_context *context, struct us_socket *(*on_data)(struct us_socket *s, char *data, int length)) {
    context->on_data = on_data;
}

void us_socket_context_on_writable(struct us_socket_context *context, struct us_socket *(*on_writable)(struct us_socket *s)) {
    context->on_writable = on_writable;
}

void us_socket_context_on_timeout(struct us_socket_context *context, struct us_socket *(*on_timeout)(struct us_socket *s)) {
    context->on_timeout = on_timeout;
}

void us_socket_context_on_end(struct us_socket_context *context, struct us_socket *(*on_end)(struct us_socket *s)) {
    context->on_end = on_end;
}

void *us_socket_context_ext(struct us_socket_context *context) {
    return context + 1;
}

struct us_listen_socket {
    int socket_ext_size;
    struct us_socket_context *context;
};

struct us_listen_socket *us_socket_context_listen(struct us_socket_context *context, const char *host, int port, int options, int socket_ext_size) {
    struct us_listen_socket *listen_socket = (struct us_listen_socket *) malloc(sizeof(struct us_listen_socket));

    listen_socket->socket_ext_size = socket_ext_size;
    listen_socket->context = context;

    context->loop->listen_socket = listen_socket;

    return listen_socket;
}

void us_listen_socket_close(struct us_listen_socket *ls) {
    free(ls);
}

struct us_socket {
    struct us_socket_context *context;
};

/* For ubsan? */
struct us_new_socket_t {
    struct us_socket_context *context;
};

struct us_socket *us_socket_context_connect(struct us_socket_context *context, const char *host, int port, int options, int socket_ext_size) {
    printf("us_socket_context_connect\n");
}

struct us_loop *us_socket_context_loop(struct us_socket_context *context) {
    return context->loop;
}

struct us_socket *us_socket_context_adopt_socket(struct us_socket_context *context, struct us_socket *s, int ext_size) {
    printf("us_socket_context_adopt_socket\n");
}

struct us_socket_context *us_create_child_socket_context(struct us_socket_context *context, int context_ext_size) {
    printf("us_create_child_socket_context\n");
}

int us_socket_write(struct us_socket *s, const char *data, int length, int msg_more) {

}

void us_socket_timeout(struct us_socket *s, unsigned int seconds) {

}

void *us_socket_ext(struct us_socket *s) {
    return s + 1;
}

struct us_socket_context *us_socket_get_context(struct us_socket *s) {
    return s->context;
}

void us_socket_flush(struct us_socket *s) {

}

void us_socket_shutdown(struct us_socket *s) {

}

int us_socket_is_shut_down(struct us_socket *s) {
    return 0;
}

int us_socket_is_closed(struct us_socket *s) {
    return 0;
}

struct us_socket *us_socket_close(struct us_socket *s) {
    return s;
}

void us_socket_remote_address(struct us_socket *s, char *buf, int *length) {
    printf("us_socket_remote_address\n");
}

/* We expose this function to let fuzz targets push data to uSockets */
void us_loop_read_mocked_data(struct us_loop *loop, char *data, unsigned int size) {

    /* We have one listen socket */
    int socket_ext_size = loop->listen_socket->socket_ext_size;

    /* Create a socket with information from the listen socket */
    struct us_socket *s = (struct us_socket *) malloc(sizeof(us_socket) + socket_ext_size);
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