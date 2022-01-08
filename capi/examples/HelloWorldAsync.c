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
    async_request_t* request_data = (async_request_t*)data;
    /* Were'nt we aborted before our async task finished? Okay, send a message! */
    if(!request_data->aborted){
        uws_res_end(request_data->res, "Hello CAPI!", 11, false);
    }
}

void get_handler(uws_res_t *res, uws_req_t *req,  void* user_data)
{

    /* We have to attach an abort handler for us to be aware
     * of disconnections while we perform async tasks */
    async_request_t* request_data = (async_request_t*) malloc(sizeof(async_request_t));
    request_data->res = res;
    request_data->aborted = false;

    uws_res_on_aborted(res, on_res_aborted, request_data);

   /* Simulate checking auth for 5 seconds. This looks like crap, never write
    * code that utilize us_timer_t like this; they are high-cost and should
    * not be created and destroyed more than rarely!
    * Either way, here we go!*/
    uws_create_timer(5000, 0, on_timer_done, request_data);
}


void listen_handler(struct us_listen_socket_t *listen_socket, uws_app_listen_config_t config,  void* user_data)
{
    if (listen_socket)
    {
        printf("Listening on port http://localhost:%d now\n", config.port);
    }
}

int main()
{
  	/* Overly simple hello world app with async response */

    uws_app_t *app = uws_create_app();
    uws_app_get(app, "/*", get_handler, NULL);
    uws_app_listen(app, 3000, listen_handler, NULL);
    uws_app_run(app);
}
