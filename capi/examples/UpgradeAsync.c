#include "../libuwebsockets.h"
#include <stdio.h>
#include <malloc.h>

/* This is a simple WebSocket "sync" upgrade example.
 * You may compile it with "WITH_OPENSSL=1 make" or with "make" */

/* uws_ws_get_user_data(ws) returns one of these */


typedef struct {
    char* value;
    size_t length;
} header_t;
struct PerSocketData
{
    /* Define your user data */
    int something;
};

struct UpgradeData
{
    header_t* secWebSocketKey;
    header_t* secWebSocketProtocol;
    header_t* secWebSocketExtensions;
    uws_socket_context_t *context;
    uws_res_t *response;
    bool aborted;
};

header_t* create_header(size_t buffer_size){
    header_t* header = (header_t*)malloc(sizeof(header_t));
    header->value = (char*)calloc(sizeof(char), buffer_size);
    header->length = 0;
    return header;
}
void free_header(header_t* header){
    
    free(header->value);
    free(header);
}
void listen_handler(struct us_listen_socket_t *listen_socket, uws_app_listen_config_t config)
{
    if (listen_socket)
    {
        printf("Listening on port ws://localhost:%d\n", config.port);
    }
}
void on_timer_done(void *data)
{

    struct UpgradeData *upgrade_data = (struct UpgradeData *)data;

    /* Were'nt we aborted before our async task finished? Okay, upgrade then! */
    if (!upgrade_data->aborted)
    {
        struct PerSocketData *socket_data = (struct PerSocketData *)malloc(sizeof(struct PerSocketData));
        socket_data->something = 15;
        printf("Async task done, upgrading to WebSocket now!\n");

        uws_res_upgrade(upgrade_data->response,
                        (void *)socket_data,
                        upgrade_data->secWebSocketKey->value,
                        upgrade_data->secWebSocketKey->length,
                        upgrade_data->secWebSocketProtocol->value,
                        upgrade_data->secWebSocketProtocol->length,
                        upgrade_data->secWebSocketExtensions->value,
                        upgrade_data->secWebSocketExtensions->length,
                        upgrade_data->context);

    }
    else
    {
        printf("Async task done, but the HTTP socket was closed. Skipping upgrade to WebSocket!\n");
    }
    free_header(upgrade_data->secWebSocketKey);
    free_header(upgrade_data->secWebSocketProtocol);
    free_header(upgrade_data->secWebSocketExtensions);
    free(upgrade_data);
}

void on_res_aborted(uws_res_t *response, void *data)
{
    struct UpgradeData *upgrade_data = (struct UpgradeData *)data;
    /* We don't implement any kind of cancellation here,
     * so simply flag us as aborted */
    upgrade_data->aborted = true;
}
void upgrade_handler(uws_res_t *response, uws_req_t *request, uws_socket_context_t *context)
{

    /* HttpRequest (req) is only valid in this very callback, so we must COPY the headers
     * we need later on while upgrading to WebSocket. You must not access req after first return.
     * Here we create a heap allocated struct holding everything we will need later on. */



    struct UpgradeData *data = (struct UpgradeData *)malloc(sizeof(struct UpgradeData));
    data->aborted = false;
    data->context = context;
    data->response = response;
    data->secWebSocketKey = create_header(100);
    data->secWebSocketProtocol = create_header(100);
    data->secWebSocketExtensions = create_header(100);

    //better check if lenght > then buffer sizes
    data->secWebSocketKey->length = uws_req_get_header(request, "sec-websocket-key", 17, data->secWebSocketKey->value, 100);
    data->secWebSocketProtocol->length = uws_req_get_header(request, "sec-websocket-protocol", 22, data->secWebSocketProtocol->value, 100);
    data->secWebSocketExtensions->length = uws_req_get_header(request, "sec-websocket-extensions", 24, data->secWebSocketExtensions->value, 100);

    /* We have to attach an abort handler for us to be aware
     * of disconnections while we perform async tasks */

    uws_res_on_aborted(response, on_res_aborted, data);

    /* Simulate checking auth for 5 seconds. This looks like crap, never write
    * code that utilize us_timer_t like this; they are high-cost and should
    * not be created and destroyed more than rarely!
    * Either way, here we go!*/
    uws_create_timer(5000, 0, on_timer_done, data);
}

void open_handler(uws_websocket_t *ws)
{

    /* Open event here, you may access uws_ws_get_user_data(ws) which points to a PerSocketData struct.
    * Here we simply validate that indeed, something == 15 as set in upgrade handler. */

    struct PerSocketData *data = (struct PerSocketData *)uws_ws_get_user_data(ws);
    data->something = 15;
    printf("Something is: %d\n", data->something);
}

void message_handler(uws_websocket_t *ws, const char *message, size_t length, uws_opcode_t opcode)
{

    /* We simply echo whatever data we get */
    uws_ws_send(ws, message, length, opcode);
}

void close_handler(uws_websocket_t *ws, int code, const char *message, size_t length)
{

    /* You may access uws_ws_get_user_data(ws) here, but sending or
     * doing any kind of I/O with the socket is not valid. */
    struct PerSocketData *data = (struct PerSocketData *)uws_ws_get_user_data(ws);
    if (data){
        free(data);
    }
}

void drain_handler(uws_websocket_t *ws)
{
    /* Check uws_ws_get_buffered_amount(ws) here */
}

void ping_handler(uws_websocket_t *ws, const char *message, size_t length)
{
    /* You don't need to handle this one, we automatically respond to pings as per standard */
}

void pong_handler(uws_websocket_t *ws, const char *message, size_t length)
{

    /* You don't need to handle this one either */
}

int main()
{

    uws_app_t *app = uws_create_app();

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