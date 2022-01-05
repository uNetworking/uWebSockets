#include "../libuwebsockets.h"
#include <stdio.h>
#include <malloc.h>

/* This is a simple WebSocket "sync" upgrade example.
 * You may compile it with "WITH_OPENSSL=1 make" or with "make" */

/* uws_ssl_ws_get_user_data(ws) returns one of these */
struct PerSocketData {
    /* Define your user data */
    int something;
};

struct UpgradeData {
    char* secWebSocketKey;
    char* secWebSocketProtocol;
    char* secWebSocketExtensions;
    uws_socket_context_t *context;
    uws_ssl_res_t* response;
    bool aborted;
};

void listen_handler(uws_listen_socket_t *listen_socket, uws_app_listen_config_t config)
{
    if (listen_socket){
        printf("Listening on port ws://localhost:%d\n", config.port);
    }
}
void on_timer_done(void* data){

    struct UpgradeData* upgrade_data =  (struct UpgradeData*)data;

    /* Were'nt we aborted before our async task finished? Okay, upgrade then! */
    if(!upgrade_data->aborted){
        struct PerSocketData* socket_data =  (struct PerSocketData*)malloc(sizeof(struct PerSocketData));
        socket_data->something = 15;
        printf("Async task done, upgrading to WebSocket now!\n");

        uws_ssl_res_upgrade(upgrade_data->response, 
                        (void*)socket_data, 
                        upgrade_data->secWebSocketKey, 
                        upgrade_data->secWebSocketProtocol, 
                        upgrade_data->secWebSocketExtensions, 
                        upgrade_data->context);

    }else{
        printf("Async task done, but the HTTP socket was closed. Skipping upgrade to WebSocket!\n");
    }

    free(upgrade_data->secWebSocketKey);
    free(upgrade_data->secWebSocketProtocol);
    free(upgrade_data->secWebSocketExtensions);
    free(upgrade_data);
}

void on_res_aborted(uws_ssl_res_t *response, void* data){
    struct UpgradeData* upgrade_data =  (struct UpgradeData*)data;
    /* We don't implement any kind of cancellation here,
     * so simply flag us as aborted */
    upgrade_data->aborted = true;

}
void upgrade_handler(uws_ssl_res_t *response, uws_req_t *request, uws_socket_context_t* context){

    /* HttpRequest (req) is only valid in this very callback, so we must COPY the headers
     * we need later on while upgrading to WebSocket. You must not access req after first return.
     * Here we create a heap allocated struct holding everything we will need later on. */

    struct UpgradeData* data =  (struct UpgradeData*)malloc(sizeof(struct UpgradeData));
    data->aborted = false;
    data->context = context;
    data->response = response;
    data->secWebSocketKey = uws_req_get_header(request, "sec-websocket-key");
    data->secWebSocketProtocol =  uws_req_get_header(request, "sec-websocket-protocol");
    data->secWebSocketExtensions = uws_req_get_header(request, "sec-websocket-extensions");

   /* We have to attach an abort handler for us to be aware
     * of disconnections while we perform async tasks */

    uws_ssl_res_on_aborted(response, on_res_aborted, data);

    /* Simulate checking auth for 5 seconds. This looks like crap, never write
    * code that utilize us_timer_t like this; they are high-cost and should
    * not be created and destroyed more than rarely!
    * Either way, here we go!*/
    uws_create_timer(5000, 0, on_timer_done, data);
}

void open_handler(uws_ssl_websocket_t* ws){

    /* Open event here, you may access uws_ssl_ws_get_user_data(ws) which points to a PerSocketData struct.
    * Here we simply validate that indeed, something == 15 as set in upgrade handler. */

    struct PerSocketData* data = (struct PerSocketData*)uws_ssl_ws_get_user_data(ws);
    data->something = 15;
    printf("Something is: %d\n", data->something);
}

void message_handler(uws_ssl_websocket_t* ws, char* message, uws_opcode_t opcode){

    /* We simply echo whatever data we get */
    uws_ssl_ws_send(ws, message, opcode);
    free(message);
}

void close_handler(uws_ssl_websocket_t* ws, int code, char* message){

    /* You may access uws_ssl_ws_get_user_data(ws) here, but sending or
     * doing any kind of I/O with the socket is not valid. */
    struct PerSocketData* data = (struct PerSocketData*)uws_ssl_ws_get_user_data(ws);
    if(data) free(data);
    free(message);
}

void drain_handler(uws_ssl_websocket_t* ws){
    /* Check uws_ssl_ws_get_buffered_amount(ws) here */
}

void ping_handler(uws_ssl_websocket_t* ws, char* message){
    /* You don't need to handle this one, we automatically respond to pings as per standard */
    free(message);
}

void pong_handler(uws_ssl_websocket_t* ws, char* message){

    /* You don't need to handle this one either */
    free(message);
}


int main()
{

    uws_ssl_app_t *app = uws_create_ssl_app((uws_socket_context_options_t){
        /* There are example certificates in uWebSockets.js repo */
	    .key_file_name = "../misc/key.pem",
	    .cert_file_name = "../misc/cert.pem",
	    .passphrase = "1234"
    });
    
	uws_ssl_ws(app, "/*", (uws_ssl_socket_behavior_t){
		.compression = uws_compress_options_t::SHARED_COMPRESSOR,
        .maxPayloadLength = 16 * 1024,
        .idleTimeout = 12,
        .maxBackpressure = 1 * 1024 * 1024,
		.upgrade = upgrade_handler,
        .open = open_handler,
        .message = message_handler,
        .drain = drain_handler,
        .ping = ping_handler,
        .pong = pong_handler,
        .close = close_handler
	});

    uws_ssl_app_listen(app, 9001, listen_handler);
    

	uws_ssl_app_run(app);
}