#include "../libuwebsockets.h"
#include <stdio.h>
#include <malloc.h>

/* This is a simple WebSocket "sync" upgrade example.
 * You may compile it with "WITH_OPENSSL=1 make" or with "make" */

/* ws->getUserData returns one of these */
struct PerSocketData {
    /* Fill with user data */
};

void listen_handler(uws_listen_socket_t *listen_socket, uws_app_listen_config_t config)
{
    if (listen_socket){
        printf("Listening on port ws://localhost:%d\n", config.port);
    }
}

void open_handler(uws_ssl_websocket_t* ws){

     /* Open event here, you may access uws_ssl_ws_get_user_data(WS) which points to a PerSocketData struct */
}

void message_handler(uws_ssl_websocket_t* ws, char* message, uws_opcode_t opcode){
    uws_ssl_ws_send(ws, message, opcode);
    free(message);
}

void close_handler(uws_ssl_websocket_t* ws, int code, char* message){

    /* You may access uws_ssl_ws_get_user_data(ws) here, but sending or
     * doing any kind of I/O with the socket is not valid. */
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
		.upgrade = NULL,
        .open = open_handler,
        .message = message_handler,
        .drain = drain_handler,
        .ping = ping_handler,
        .pong = pong_handler,
        .close = close_handler,
	});

    uws_ssl_app_listen(app, 9001, listen_handler);
    

	uws_ssl_app_run(app);
}