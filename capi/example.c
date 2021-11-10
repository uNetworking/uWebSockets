#include "libuwebsockets.h"
#include <stdio.h>

void get_handler(uws_res_t *res, uws_req_t *req) {
    uws_res_end(res, "Hello CAPI!", 11);
}

void listen_handler(void *listen_socket) {
    if (listen_socket) {
        printf("Listening on port now\n");
    }
}

int main() {
    uws_app_t *app = uws_create_app();
    uws_app_get(app, "/*", get_handler);
    uws_app_listen(app, 3000, listen_handler);
    uws_app_run(app);
}