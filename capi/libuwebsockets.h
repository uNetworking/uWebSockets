
#ifndef LIBUWS_CAPI_HEADER
#define LIBUWS_CAPI_HEADER

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "libusockets.h"

#ifdef __cplusplus
extern "C"
{
#endif

   typedef enum
    {
        /* These are not actual compression options */
        _COMPRESSOR_MASK = 0x00FF,
        _DECOMPRESSOR_MASK = 0x0F00,
        /* Disabled, shared, shared are "special" values */
        DISABLED = 0,
        SHARED_COMPRESSOR = 1,
        SHARED_DECOMPRESSOR = 1 << 8,
        /* Highest 4 bits describe decompressor */
        DEDICATED_DECOMPRESSOR_32KB = 15 << 8,
        DEDICATED_DECOMPRESSOR_16KB = 14 << 8,
        DEDICATED_DECOMPRESSOR_8KB = 13 << 8,
        DEDICATED_DECOMPRESSOR_4KB = 12 << 8,
        DEDICATED_DECOMPRESSOR_2KB = 11 << 8,
        DEDICATED_DECOMPRESSOR_1KB = 10 << 8,
        DEDICATED_DECOMPRESSOR_512B = 9 << 8,
        /* Same as 32kb */
        DEDICATED_DECOMPRESSOR = 15 << 8,

        /* Lowest 8 bit describe compressor */
        DEDICATED_COMPRESSOR_3KB = 9 << 4 | 1,
        DEDICATED_COMPRESSOR_4KB = 9 << 4 | 2,
        DEDICATED_COMPRESSOR_8KB = 10 << 4 | 3,
        DEDICATED_COMPRESSOR_16KB = 11 << 4 | 4,
        DEDICATED_COMPRESSOR_32KB = 12 << 4 | 5,
        DEDICATED_COMPRESSOR_64KB = 13 << 4 | 6,
        DEDICATED_COMPRESSOR_128KB = 14 << 4 | 7,
        DEDICATED_COMPRESSOR_256KB = 15 << 4 | 8,
        /* Same as 256kb */
        DEDICATED_COMPRESSOR = 15 << 4 | 8
    } uws_compress_options_t;

    typedef enum
    {
        CONTINUATION = 0,
        TEXT = 1,
        BINARY = 2,
        CLOSE = 8,
        PING = 9,
        PONG = 10
    } uws_opcode_t;

    typedef enum
    {
        BACKPRESSURE,
        SUCCESS,
        DROPPED
    } uws_sendstatus_t;

     typedef struct
    {
        const char *key_file_name;
        const char *cert_file_name;
        const char *passphrase;
        const char *dh_params_file_name;
        const char *ca_file_name;
        int ssl_prefer_low_memory_usage;
    } uws_socket_context_options_t;
  


    typedef struct
    {

        int port;
        const char *host;
        int options;
    } uws_app_listen_config_t;
    typedef struct uws_timer_s uws_timer_t;

    struct uws_app_s;
    struct uws_req_s;
    struct uws_res_s;
    struct uws_websocket_s;
    struct uws_header_iterator_s;
    typedef struct uws_app_s uws_app_t;
    typedef struct uws_req_s uws_req_t;
    typedef struct uws_res_s uws_res_t;
    typedef struct uws_socket_context_s uws_socket_context_t;
    typedef struct uws_websocket_s uws_websocket_t;
   

    typedef void (*uws_websocket_handler)(uws_websocket_t * ws);
    typedef void (*uws_websocket_message_handler)(uws_websocket_t * ws, const char* message, size_t length, uws_opcode_t opcode);
    typedef void (*uws_websocket_ping_pong_handler)(uws_websocket_t * ws, const char* message, size_t length);
    typedef void (*uws_websocket_close_handler)(uws_websocket_t * ws, int code, const char* message, size_t length);
    typedef void (*uws_websocket_upgrade_handler)(uws_res_t *response, uws_req_t *request, uws_socket_context_t* context);



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

        uws_websocket_upgrade_handler upgrade;
        uws_websocket_handler open;
        uws_websocket_message_handler message;
        uws_websocket_handler drain;
        uws_websocket_ping_pong_handler ping;
        uws_websocket_ping_pong_handler pong;
        uws_websocket_close_handler close;
    } uws_socket_behavior_t;
    
    typedef void (*uws_listen_handler)(struct us_listen_socket_t *listen_socket, uws_app_listen_config_t config);
    typedef void (*uws_method_handler)(uws_res_t *response, uws_req_t *request);
    typedef void (*uws_filter_handler)(uws_res_t *response, int);
    typedef void (*uws_missing_server_handler)(const char *hostname);
    //Basic HTTP
    uws_app_t *uws_create_app();
    void uws_app_destroy(uws_app_t *app);
    void uws_app_get(uws_app_t *app, const char *pattern, uws_method_handler handler);
    void uws_app_post(uws_app_t *app, const char *pattern, uws_method_handler handler);
    void uws_app_options(uws_app_t *app, const char *pattern, uws_method_handler handler);
    void uws_app_delete(uws_app_t *app, const char *pattern, uws_method_handler handler);
    void uws_app_patch(uws_app_t *app, const char *pattern, uws_method_handler handler);
    void uws_app_put(uws_app_t *app, const char *pattern, uws_method_handler handler);
    void uws_app_head(uws_app_t *app, const char *pattern, uws_method_handler handler);
    void uws_app_connect(uws_app_t *app, const char *pattern, uws_method_handler handler);
    void uws_app_trace(uws_app_t *app, const char *pattern, uws_method_handler handler);
    void uws_app_any(uws_app_t *app, const char *pattern, uws_method_handler handler);

    void uws_app_run(uws_app_t *);

    void uws_app_listen(uws_app_t *app, int port, uws_listen_handler handler);
    void uws_app_listen_with_config(uws_app_t *app, uws_app_listen_config_t config, uws_listen_handler handler);
    bool uws_constructor_failed(uws_app_t *app);
    unsigned int uws_num_subscribers(uws_app_t *app, const char *topic);
    bool uws_publish(uws_app_t *app, const char *topic, size_t topic_length, const char *message, size_t message_length, uws_opcode_t opcode, bool compress);
    void *uws_get_native_handle(uws_app_t *app);
    void uws_remove_server_name(uws_app_t *app, const char *hostname_pattern);
    void uws_add_server_name(uws_app_t *app, const char *hostname_pattern);
    void uws_add_server_name_with_options(uws_app_t *app, const char *hostname_pattern, uws_socket_context_options_t options);
    void uws_missing_server_name(uws_app_t *app, uws_missing_server_handler handler);
    void uws_filter(uws_app_t *app, uws_filter_handler handler);
    
    //WebSocket
    void uws_ws(uws_app_t *app, const char *pattern, uws_socket_behavior_t behavior);
    void* uws_ws_get_user_data(uws_websocket_t* ws);
    void uws_ws_close(uws_websocket_t* ws);
    uws_sendstatus_t uws_ws_send(uws_websocket_t *ws, const char *message, size_t length, uws_opcode_t opcode);
    uws_sendstatus_t uws_ws_send_with_options(uws_websocket_t* ws, const char* message, size_t length, uws_opcode_t opcode, bool compress, bool fin);
    uws_sendstatus_t uws_ws_send_fragment(uws_websocket_t* ws, const char* message, size_t length, bool compress);
    uws_sendstatus_t uws_ws_send_first_fragment(uws_websocket_t* ws, const char* message, size_t length, bool compress);
    uws_sendstatus_t uws_ws_send_first_fragment_with_opcode(uws_websocket_t* ws, const char* message, size_t length, uws_opcode_t opcode, bool compress);
    uws_sendstatus_t uws_ws_send_last_fragment(uws_websocket_t* ws, const char* message, size_t length, bool compress);
    void uws_ws_end(uws_websocket_t* ws,  int code, const char* message, size_t length);
    void uws_ws_cork(uws_websocket_t* ws, void(*handler)());
    bool uws_ws_subscribe(uws_websocket_t* ws,  const char* topic, size_t length);
    bool uws_ws_unsubscribe(uws_websocket_t* ws,  const char* topic, size_t length);
    bool uws_ws_is_subscribed(uws_websocket_t* ws,  const char* topic, size_t length);
    void uws_ws_iterate_topics(uws_websocket_t* ws,  void(*callback)(const char* topic, size_t length));
    bool uws_ws_publish(uws_websocket_t *ws, const char *topic, size_t topic_length, const char *message, size_t message_length);
    bool uws_ws_publish_with_options(uws_websocket_t *ws, const char *topic, size_t topic_length, const char *message, size_t message_length, uws_opcode_t opcode, bool compress);
    unsigned int uws_ws_get_buffered_amount(uws_websocket_t* ws);
    int uws_ws_get_remote_address(uws_websocket_t* ws, char* dest_buffer, size_t dest_buffer_length);
    int uws_ws_get_remote_address_as_text(uws_websocket_t* ws, char* dest_buffer, size_t dest_buffer_length);

    //Response
    void uws_res_end(uws_res_t *res, const char *data, size_t length, bool close_connection);
    void uws_res_pause(uws_res_t *res);
    void uws_res_resume(uws_res_t *res);
    void uws_res_write_continue(uws_res_t *res);
    void uws_res_write_status(uws_res_t *res, const char *status, size_t length);
    void uws_res_write_header(uws_res_t *res, const char *key,  size_t key_length, const char* value, size_t value_length);
    void uws_res_write_header_int(uws_res_t *res, const char *key, size_t key_length, uint64_t value);
    void uws_res_end_without_body(uws_res_t *res);
    bool uws_res_write(uws_res_t *res, const char *data, size_t length);
    uintmax_t uws_res_get_write_offset(uws_res_t *res);
    bool uws_res_has_responded(uws_res_t *res);
    void uws_res_on_writable(uws_res_t *res,  bool (*handler)(uws_res_t *res, uintmax_t, void* opcional_data), void* opcional_data);
    void uws_res_on_aborted(uws_res_t *res,  void (*handler)(uws_res_t *res, void* opcional_data), void* opcional_data);
    void uws_res_on_data(uws_res_t *res, void (*handler)(uws_res_t *res, const char* chunk, size_t chunk_length, bool is_end, void* opcional_data), void* opcional_data);
    void uws_res_upgrade(uws_res_t *res, void *data, const char *sec_web_socket_key, size_t sec_web_socket_key_length, const char *sec_web_socket_protocol, size_t sec_web_socket_protocol_length, const char *sec_web_socket_extensions, size_t sec_web_socket_extensions_length, uws_socket_context_t *ws);
    
    //Request
    bool uws_req_is_ancient(uws_req_t *res);
    bool uws_req_get_yield(uws_req_t *res);
    void uws_req_set_field(uws_req_t *res, bool yield);
    int uws_req_get_url(uws_req_t *res, char* dest_buffer, size_t dest_buffer_length);
    int uws_req_get_method(uws_req_t *res, char* dest_buffer, size_t dest_buffer_length);
    int uws_req_get_header(uws_req_t *res, const char* lower_case_header, size_t lower_case_header_length, char* dest_buffer, size_t dest_buffer_length);
    int uws_req_get_query(uws_req_t *res, const char* key, size_t key_length, char* dest_buffer, size_t dest_buffer_length);
    int uws_req_get_parameter(uws_req_t *res, unsigned short index, char* dest_buffer, size_t dest_buffer_length);

    //Eventing
    uws_timer_t *uws_create_timer(int ms, int repeat_ms, void (*handler)(void *data), void *data);
    void uws_timer_close(uws_timer_t *timer);

#ifdef __cplusplus
}
#endif

#endif