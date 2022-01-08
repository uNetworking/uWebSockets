#include "../libuwebsockets.h"
#include <stdio.h>

void get_handler(uws_res_t *res, uws_req_t *req, void *user_data)
{
    uws_res_end(res, "Hello CAPI!", 11, false);
}

void listen_handler(struct us_listen_socket_t *listen_socket, uws_app_listen_config_t config, void *user_data)
{
    if (listen_socket)
    {
        printf("Listening on port http://localhost:%d now\n", config.port);
    }
}

int main()
{
    /* Overly simple hello world app */

    uws_app_t *app = uws_create_app();
    uws_app_get(app, "/*", get_handler, NULL);
    uws_app_listen(app, 3000, listen_handler, NULL);
    uws_app_run(app);
}
