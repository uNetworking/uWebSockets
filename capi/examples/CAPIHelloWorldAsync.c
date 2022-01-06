#include "../libuwebsockets.h"
#include <stdio.h>
#include <malloc.h>

typedef struct {
    uws_res_t* res;
    bool aborted;
} async_request_t;

void on_res_aborted(uws_res_t *response, void* data){
    async_request_t* request_data =  (async_request_t*)data;
    /* We don't implement any kind of cancellation here,
     * so simply flag us as aborted */
    request_data->aborted = true;
}

void on_timer_done(void *data){
    async_request_t* info = (async_request_t*)data;
    uws_res_end(info->res, "Hello CAPI!", false);
}

void get_handler(uws_res_t *res, uws_req_t *req)
{
    async_request_t* request_data = (async_request_t*) malloc(sizeof(async_request_t));
    request_data->res = res;
    request_data->aborted = false;
    //simulate 5s async delay
    uws_create_timer(5000, 0, on_timer_done, request_data);
    //attach to abort handler
    uws_res_on_aborted(res, on_res_aborted, request_data);
}


void listen_handler(uws_listen_socket_t *listen_socket, uws_app_listen_config_t config)
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
    uws_app_get(app, "/*", get_handler);
    uws_app_listen(app, 3000, listen_handler);
    uws_app_run(app);
}
