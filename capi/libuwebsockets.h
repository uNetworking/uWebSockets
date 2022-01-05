
#ifndef LIBUWS_CAPI_HEADER
#define LIBUS_CAPI_HEADER

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    enum uws_compress_options_t
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
    };

    enum uws_opcode_t
    {
        CONTINUATION = 0,
        TEXT = 1,
        BINARY = 2,
        CLOSE = 8,
        PING = 9,
        PONG = 10
    };

    enum uws_sendstatus_t
    {
        BACKPRESSURE,
        SUCCESS,
        DROPPED
    };

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
    //Eventing
    uws_timer_t *uws_create_timer(int ms, int repeat_ms, void (*handler)(void *data), void *data);
    void uws_timer_close(uws_timer_t *timer);

#ifdef __cplusplus
}
#endif

#include "CAPIApp.h"
#include "CAPIAppSSL.h"

#endif