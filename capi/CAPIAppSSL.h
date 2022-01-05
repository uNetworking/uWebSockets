#ifndef LIBUWS_CAPI_HEADER_APP
#define LIBUS_CAPI_HEADER_APP

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif

    struct uws_ssl_app_s;
    struct uws_ssl_res_s;
    struct uws_ssl_websocket_s;
    typedef struct uws_ssl_app_s uws_ssl_app_t;
    typedef struct uws_ssl_ssl_app_s uws_ssl_ssl_app_t;
    typedef struct uws_ssl_res_s uws_ssl_res_t;
    typedef struct uws_ssl_websocket_s uws_ssl_websocket_t;
   
    typedef void (*uws_listen_handler)(uws_listen_socket_t *listen_socket, uws_app_listen_config_t config);
    typedef void (*uws_ssl_method_handler)(uws_ssl_res_t *response, uws_req_t *request);
    typedef void (*uws_ssl_filter_handler)(uws_ssl_res_t *response, int);


    typedef void (*uws_ssl_websocket_handler)(uws_ssl_websocket_t * ws);
    typedef void (*uws_ssl_websocket_message_handler)(uws_ssl_websocket_t * ws, char* message, uws_opcode_t opcode);
    typedef void (*uws_ssl_websocket_ping_pong_handler)(uws_ssl_websocket_t * ws, char* message);
    typedef void (*uws_ssl_websocket_close_handler)(uws_ssl_websocket_t * ws, int code, char* message);
    typedef void (*uws_ssl_websocket_upgrade_handler)(uws_ssl_res_t *response, uws_req_t *request, uws_socket_context_t* context);


    typedef struct
    {
        uws_compress_options_t compression;
        /* Maximum message size we can receive */
        unsigned int maxPayloadLength;
        /* 2 minutes timeout is good */
        unsigned short idleTimeout;
        /* 64kb backpressure is probably good */
        unsigned int maxBackpressure;
        bool closeOnBackpressureLimit;
        /* This one depends on kernel timeouts and is a bad default */
        bool resetIdleTimeoutOnSend;
        /* A good default, esp. for newcomers */
        bool sendPingsAutomatically;
        /* Maximum socket lifetime in seconds before forced closure (defaults to disabled) */
        unsigned short maxLifetime;

        uws_ssl_websocket_upgrade_handler upgrade;
        uws_ssl_websocket_handler open;
        uws_ssl_websocket_message_handler message;
        uws_ssl_websocket_handler drain;
        uws_ssl_websocket_ping_pong_handler ping;
        uws_ssl_websocket_ping_pong_handler pong;
        uws_ssl_websocket_close_handler close;
    } uws_ssl_socket_behavior_t;
    

    //Basic HTTPS
    uws_ssl_app_t *uws_create_ssl_app(uws_socket_context_options_t options);
    void uws_ssl_app_destroy(uws_ssl_app_t *app);
    void uws_ssl_app_get(uws_ssl_app_t *app, const char *pattern, uws_ssl_method_handler handler);
    void uws_ssl_app_post(uws_ssl_app_t *app, const char *pattern, uws_ssl_method_handler handler);
    void uws_ssl_app_options(uws_ssl_app_t *app, const char *pattern, uws_ssl_method_handler handler);
    void uws_ssl_app_delete(uws_ssl_app_t *app, const char *pattern, uws_ssl_method_handler handler);
    void uws_ssl_app_patch(uws_ssl_app_t *app, const char *pattern, uws_ssl_method_handler handler);
    void uws_ssl_app_put(uws_ssl_app_t *app, const char *pattern, uws_ssl_method_handler handler);
    void uws_ssl_app_head(uws_ssl_app_t *app, const char *pattern, uws_ssl_method_handler handler);
    void uws_ssl_app_connect(uws_ssl_app_t *app, const char *pattern, uws_ssl_method_handler handler);
    void uws_ssl_app_trace(uws_ssl_app_t *app, const char *pattern, uws_ssl_method_handler handler);
    void uws_ssl_app_any(uws_ssl_app_t *app, const char *pattern, uws_ssl_method_handler handler);
    void uws_ssl_app_run(uws_ssl_app_t *app);
    void uws_ssl_app_listen(uws_ssl_app_t *app, int port, uws_listen_handler handler);
    void uws_ssl_app_listen_with_config(uws_ssl_app_t *app, uws_app_listen_config_t config, uws_listen_handler handler);
    bool uws_ssl_constructor_failed(uws_ssl_app_t *app);
    unsigned int uws_ssl_num_subscribers(uws_ssl_app_t *app, const char *topic);
    bool uws_ssl_publish(uws_ssl_app_t *app, const char *topic, const char *message, uws_opcode_t opcode, bool compress);
    void *uws_ssl_get_native_handle(uws_ssl_app_t *app);
    void uws_ssl_remove_server_name(uws_ssl_app_t *app, const char *hostname_pattern);
    void uws_ssl_add_server_name(uws_ssl_app_t *app, const char *hostname_pattern);
    void uws_ssl_add_server_name_with_options(uws_ssl_app_t *app, const char *hostname_pattern, uws_socket_context_options_t options);
    void uws_ssl_missing_server_name(uws_ssl_app_t *app, uws_missing_server_handler handler);
    void uws_ssl_filter(uws_ssl_app_t *app, uws_ssl_filter_handler handler);

    //WebSocket
    void uws_ssl_ws(uws_ssl_app_t *app, const char *pattern, uws_ssl_socket_behavior_t behavior);
    void *uws_ssl_ws_get_user_data(uws_ssl_websocket_t *ws);
    void uws_ssl_ws_close(uws_ssl_websocket_t *ws);
    uws_sendstatus_t uws_ssl_ws_send(uws_ssl_websocket_t *ws, const char *message, uws_opcode_t opcode);
    uws_sendstatus_t uws_ssl_ws_send_with_options(uws_ssl_websocket_t *ws, const char *message, uws_opcode_t opcode, bool compress, bool fin);
    uws_sendstatus_t uws_ssl_ws_send_fragment(uws_ssl_websocket_t *ws, const char *message, bool compress);
    uws_sendstatus_t uws_ssl_ws_send_first_fragment(uws_ssl_websocket_t *ws, const char *message, bool compress);
    uws_sendstatus_t uws_ssl_ws_send_first_fragment_with_opcode(uws_ssl_websocket_t *ws, const char *message, uws_opcode_t opcode, bool compress);
    uws_sendstatus_t uws_ssl_ws_send_last_fragment(uws_ssl_websocket_t *ws, const char *message, bool compress);
    void uws_ssl_ws_end(uws_ssl_websocket_t *ws, int code, const char *message);
    void uws_ssl_ws_cork(uws_ssl_websocket_t *ws, void (*handler)());
    bool uws_ssl_ws_subscribe(uws_ssl_websocket_t *ws, const char *topic);
    bool uws_ssl_ws_unsubscribe(uws_ssl_websocket_t *ws, const char *topic);
    bool uws_ssl_ws_is_subscribed(uws_ssl_websocket_t *ws, const char *topic);
    void uws_ssl_ws_iterate_topics(uws_ssl_websocket_t *ws, void (*callback)(const char *topic));
    bool uws_ssl_ws_publish(uws_ssl_websocket_t *ws, const char *topic, const char *message);
    bool uws_ssl_ws_publish_with_options(uws_ssl_websocket_t *ws, const char *topic, const char *message, uws_opcode_t opcode, bool compress);
    unsigned int uws_ssl_ws_get_buffered_amount(uws_ssl_websocket_t *ws);
    char *uws_ssl_ws_get_remote_address(uws_ssl_websocket_t *ws);
    char *uws_ssl_ws_get_remote_address_as_text(uws_ssl_websocket_t *ws);

    //Response
    void uws_ssl_res_end(uws_ssl_res_t *res, const char *data, bool close_connection);
    void uws_ssl_res_pause(uws_ssl_res_t *res);
    void uws_ssl_res_resume(uws_ssl_res_t *res);
    void uws_ssl_res_write_continue(uws_ssl_res_t *res);
    void uws_ssl_res_write_status(uws_ssl_res_t *res, const char *status);
    void uws_ssl_res_write_header(uws_ssl_res_t *res, const char *key, const char *value);
    void uws_ssl_res_write_header_int(uws_ssl_res_t *res, const char *key, uint64_t value);
    void uws_ssl_res_end_without_body(uws_ssl_res_t *res);
    bool uws_ssl_res_write(uws_ssl_res_t *res, const char *data);
    uintmax_t uws_ssl_res_get_write_offset(uws_ssl_res_t *res);
    bool uws_ssl_res_has_responded(uws_ssl_res_t *res);
    void uws_ssl_res_on_writable(uws_ssl_res_t *res, bool (*handler)(uws_ssl_res_t *res, uintmax_t, void* opcional_data), void* opcional_data);
    void uws_ssl_res_on_aborted(uws_ssl_res_t *res, void (*handler)(uws_ssl_res_t *res, void* opcional_data), void* opcional_data);
    void uws_ssl_res_on_data(uws_ssl_res_t *res, void (*handler)(uws_ssl_res_t *res, const char *chunk, bool is_end, void* opcional_data), void* opcional_data);
    void uws_ssl_res_upgrade(uws_ssl_res_t *res, void *data, const char *sec_web_socket_key, const char *sec_web_socket_protocol, const char *sec_web_socket_extensions, uws_socket_context_t *ws);

#ifdef __cplusplus
}
#endif

#endif