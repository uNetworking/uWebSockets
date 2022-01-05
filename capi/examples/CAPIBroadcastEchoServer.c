#include "../libuwebsockets.h"
#include <stdio.h>
#include <malloc.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>

/* This is a simple WebSocket "sync" upgrade example.
 * You may compile it with "WITH_OPENSSL=1 make" or with "make" */

/* ws->getUserData returns one of these */
struct PerSocketData {
    /* Fill with user data */
    char** topics;
    int topics_quantity;
    int nr;
};

uws_app_t *app;

int buffer_size(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsnprintf(NULL, 0, format, args);
    va_end(args);
    return result + 1; // safe byte for \0
}

void listen_handler(uws_listen_socket_t *listen_socket, uws_app_listen_config_t config)
{
    if (listen_socket){
        printf("Listening on port ws://localhost:%d\n", config.port);
    }
}

void upgrade_handler(uws_res_t *response, uws_req_t *request, uws_socket_context_t* context){

    /* You may read from req only here, and COPY whatever you need into your PerSocketData.
     * PerSocketData is valid from .open to .close event, accessed with uws_ws_get_user_data(ws).
     * HttpRequest (req) is ONLY valid in this very callback, so any data you will need later
     * has to be COPIED into PerSocketData here. */

    /* Immediately upgrading without doing anything "async" before, is simple */
            
    struct PerSocketData* data =  (struct PerSocketData*)malloc(sizeof(struct PerSocketData));
    data->topics = (char**)calloc(32, sizeof(char*));
    data->topics_quantity = 32;
    data->nr = 0;

    char* ws_key = uws_req_get_header(request, "sec-websocket-key");
    char* ws_protocol = uws_req_get_header(request, "sec-websocket-protocol");
    char* ws_extensions = uws_req_get_header(request, "sec-websocket-extensions");

    uws_res_upgrade(response, 
                    (void*)data, 
                    ws_key, 
                    ws_protocol, 
                    ws_extensions, 
                    context);

    free(ws_key);
    free(ws_protocol);
    free(ws_extensions);

}


void open_handler(uws_websocket_t* ws){
      
      /* Open event here, you may access  uws_ws_get_user_data(ws) which points to a PerSocketData struct */

     struct PerSocketData* data = (struct PerSocketData*)uws_ws_get_user_data(ws);
      for (int i = 0; i < data->topics_quantity; i++) {

           char* topic = (char*)malloc((size_t)buffer_size("%ld-%d", (uintptr_t)ws, i));
           sprintf(topic, "%ld-%d", (uintptr_t)ws, i);
           data->topics[i] = topic;
           uws_ws_subscribe(ws, topic);
       }
}

void message_handler(uws_websocket_t* ws, char* message, uws_opcode_t opcode){

    struct PerSocketData* data = (struct PerSocketData*)uws_ws_get_user_data(ws);
    uws_publish(app, data->topics[(size_t)(++data->nr % 32)], message, opcode, false);
    uws_ws_publish(ws, data->topics[(size_t)(++data->nr % 32)], message);

    free(message);
}

void close_handler(uws_websocket_t* ws, int code, char* message){
    /* You may access uws_ws_get_user_data(ws) here, but sending or
     * doing any kind of I/O with the socket is not valid. */
    struct PerSocketData* data = (struct PerSocketData*)uws_ws_get_user_data(ws);
    if(data){
        for(int i = 0; i < data->topics_quantity; i++){
            free(data->topics[i]);
        }
        free(data->topics);
        free(data);
    }
    free(message);
}

void drain_handler(uws_websocket_t* ws){
    /* Check uws_ws_get_buffered_amount(ws) here */
}

void ping_handler(uws_websocket_t* ws, char* message){
    /* You don't need to handle this one, we automatically respond to pings as per standard */
    free(message);
}

void pong_handler(uws_websocket_t* ws, char* message){

    /* You don't need to handle this one either */
    free(message);
}


int main()
{

    app = uws_create_app();

	uws_ws(app, "/*", (uws_socket_behavior_t){
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
        .close = close_handler,
	});

    uws_app_listen(app, 9001, listen_handler);

	uws_app_run(app);
}