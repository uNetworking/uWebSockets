#define _DEFAULT_SOURCE

#ifdef __APPLE__
#include <libkern/OSByteOrder.h>
#define htobe16(x) OSSwapHostToBigInt16(x)
#define htole16(x) OSSwapHostToLittleInt16(x)
#define be16toh(x) OSSwapBigToHostInt16(x)
#define le16toh(x) OSSwapLittleToHostInt16(x)
#define htobe32(x) OSSwapHostToBigInt32(x)
#define htole32(x) OSSwapHostToLittleInt32(x)
#define be32toh(x) OSSwapBigToHostInt32(x)
#define le32toh(x) OSSwapLittleToHostInt32(x)
#define htobe64(x) OSSwapHostToBigInt64(x)
#define htole64(x) OSSwapHostToLittleInt64(x)
#define be64toh(x) OSSwapBigToHostInt64(x)
#define le64toh(x) OSSwapLittleToHostInt64(x)
#else
#include <endian.h>
#endif

#include <stdint.h>
#include <libusockets.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <time.h>

/** @brief Structure to hold per-socket state */
typedef struct {
    int offset;            // Bytes sent of the WebSocket request
    int upgrade_offset;    // Bytes sent of the upgrade request
    int is_upgraded;       // Whether the socket has been upgraded to WebSocket
    int outstanding_bytes; // Expected bytes to receive for echo completion
} SocketState;

/** @brief Structure to hold benchmark configuration */
typedef struct {
    char *host;                          // Target server host
    int port;                            // Target server port
    int SSL;                             // Whether to use SSL (0 or 1)
    int deflate;                         // Whether to use compression (0 or 1)
    int payload_size;                    // Size of the payload in bytes
    unsigned char *web_socket_request;   // Precomputed WebSocket request frame
    int web_socket_request_size;         // Size of the request frame
    int web_socket_request_response_size;// Expected response size
    char *upgrade_request;               // HTTP upgrade request string
    int upgrade_request_length;          // Length of the upgrade request
} BenchmarkConfig;

// Static variables for configuration and shared state
static BenchmarkConfig config;
static int connections;  // Number of remaining connections to establish
static int responses = 0;// Number of responses received

// Predefined static data
static char request_deflate[] = "GET / HTTP/1.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\nSec-WebSocket-Extensions: permessage-deflate\r\nHost: server.example.com\r\nSec-WebSocket-Version: 13\r\n\r\n";
static char request_text[] = "GET / HTTP/1.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\nHost: server.example.com\r\nSec-WebSocket-Version: 13\r\n\r\n";

/**
 * @brief Initializes a text WebSocket request
 * @param size Payload size in bytes
 * @param config Pointer to the BenchmarkConfig to populate
 */
void init_text_request(int size, BenchmarkConfig *config) {
    if (size <= 125) {
        config->web_socket_request_size = size + 6;
        config->web_socket_request = malloc(config->web_socket_request_size);
        config->web_socket_request[0] = 130; // FIN + opcode 1 (text)
        config->web_socket_request[1] = 128 | size; // Mask bit + length
        config->web_socket_request[2] = 1;
        config->web_socket_request[3] = 2;
        config->web_socket_request[4] = 3;
        config->web_socket_request[5] = 4;
        memset(config->web_socket_request + 6, 'T', size);
    } else if (size <= 65535) {
        config->web_socket_request_size = size + 8;
        config->web_socket_request = malloc(config->web_socket_request_size);
        config->web_socket_request[0] = 130;
        config->web_socket_request[1] = 128 | 126;
        uint16_t len = htobe16(size);
        memcpy(&config->web_socket_request[2], &len, 2);
        config->web_socket_request[4] = 1;
        config->web_socket_request[5] = 2;
        config->web_socket_request[6] = 3;
        config->web_socket_request[7] = 4;
        memset(config->web_socket_request + 8, 'T', size);
    } else {
        config->web_socket_request_size = size + 14;
        config->web_socket_request = malloc(config->web_socket_request_size);
        config->web_socket_request[0] = 130;
        config->web_socket_request[1] = 128 | 127;
        uint64_t len = htobe64(size);
        memcpy(&config->web_socket_request[2], &len, 8);
        config->web_socket_request[10] = 1;
        config->web_socket_request[11] = 2;
        config->web_socket_request[12] = 3;
        config->web_socket_request[13] = 4;
        memset(config->web_socket_request + 14, 'T', size);
    }
    config->web_socket_request_response_size = config->web_socket_request_size;
}

/**
 * @brief Initializes a deflated WebSocket request
 * @param size Size of the uncompressed payload in bytes
 * @param config Pointer to the BenchmarkConfig to populate
 */
void init_deflated_request(int size, BenchmarkConfig *config) {
    const char placeholder[] = "{\"userId\":12345,\"action\":\"purchase\",\"items\":[{\"id\":\"A1B2C3\",\"name\":\"Wireless Mouse\",\"price\":25.99,\"quantity\":1},{\"id\":\"D4E5F6\",\"name\":\"Mechanical Keyboard\",\"price\":89.99,\"quantity\":1}],\"payment\":{\"method\":\"credit_card\",\"transactionId\":\"XYZ987654321\",\"status\":\"approved\"},\"timestamp\":\"2025-02-20T15:30:00Z\"};";
    char *json_message = malloc(size);
    int placeholder_len = sizeof(placeholder) - 1;
    printf("Using placeholder of %d bytes\n", placeholder_len);
    for (int i = 0; i < size; i += placeholder_len) {
        int copy_len = (i + placeholder_len <= size) ? placeholder_len : size - i;
        memcpy(json_message + i, placeholder, copy_len);
    }

    z_stream defstream;
    defstream.zalloc = Z_NULL;
    defstream.zfree = Z_NULL;
    defstream.opaque = Z_NULL;
    deflateInit2(&defstream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    uLongf compressed_max_size = deflateBound(&defstream, (uLong)size);
    Bytef *compressed_data = malloc(compressed_max_size);

    defstream.avail_in = (uInt)size;
    defstream.next_in = (Bytef *)json_message;
    defstream.avail_out = (uInt)compressed_max_size;
    defstream.next_out = compressed_data;
    int res = deflate(&defstream, Z_SYNC_FLUSH);
    if (res != Z_OK) {
        printf("Deflation failed: %d\n", res);
        exit(1);
    }
    uLongf compressed_size = defstream.total_out - 4;
    deflateEnd(&defstream);
    free(json_message);

    unsigned char mask[4] = {0, 0, 0, 0};
    for (uLongf i = 0; i < compressed_size; i++) {
        compressed_data[i] ^= mask[i % 4];
    }

    unsigned char header[14];
    int header_len;
    if (compressed_size <= 125) {
        header_len = 6;
        header[0] = 0xC2; // FIN + RSV1 + Opcode 2
        header[1] = 0x80 | compressed_size;
        memcpy(header + 2, mask, 4);
    } else if (compressed_size <= 65535) {
        header_len = 8;
        header[0] = 0xC2;
        header[1] = 0x80 | 126;
        header[2] = (compressed_size >> 8) & 0xFF;
        header[3] = compressed_size & 0xFF;
        memcpy(header + 4, mask, 4);
    } else {
        header_len = 14;
        header[0] = 0xC2;
        header[1] = 0x80 | 127;
        for (int i = 0; i < 8; i++) {
            header[2 + i] = (compressed_size >> (56 - i * 8)) & 0xFF;
        }
        memcpy(header + 10, mask, 4);
    }

    config->web_socket_request_size = header_len + compressed_size;
    config->web_socket_request = malloc(config->web_socket_request_size);
    memcpy(config->web_socket_request, header, header_len);
    memcpy(config->web_socket_request + header_len, compressed_data, compressed_size);
    free(compressed_data);

    int server_will_compress = 0;
    if (server_will_compress) {
        config->web_socket_request_response_size = config->web_socket_request_size;
    } else {
        if (size <= 125) {
            config->web_socket_request_response_size = 6 + size;
        } else if (size <= 65535) {
            config->web_socket_request_response_size = 8 + size;
        } else {
            config->web_socket_request_response_size = 14 + size;
        }
    }
}

/**
 * @brief Initiates the next connection or starts the benchmark
 * @param s Current socket
 */
void next_connection(struct us_socket_t *s) {
    if (--connections) {
        us_socket_context_connect(config.SSL, us_socket_context(config.SSL, s), config.host, config.port, NULL, 0, sizeof(SocketState));
    } else {
        printf("Running benchmark now...\n");
        us_socket_timeout(config.SSL, s, LIBUS_TIMEOUT_GRANULARITY);
    }
}

/** @brief Handler for when a socket becomes writable */
struct us_socket_t *on_http_socket_writable(struct us_socket_t *s) {
    SocketState *state = (SocketState *)us_socket_ext(config.SSL, s);
    if (state->upgrade_offset < config.upgrade_request_length) {
        state->upgrade_offset += us_socket_write(config.SSL, s, config.upgrade_request + state->upgrade_offset, config.upgrade_request_length - state->upgrade_offset, 0);
        if (state->upgrade_offset == config.upgrade_request_length) {
            next_connection(s);
        }
    } else {
        state->offset += us_socket_write(config.SSL, s, (char *)config.web_socket_request + state->offset, config.web_socket_request_size - state->offset, 0);
    }
    return s;
}

/** @brief Handler for socket closure */
struct us_socket_t *on_http_socket_close(struct us_socket_t *s, int code, void *reason) {
    printf("Closed!\n");
    return s;
}

/** @brief Handler for socket end */
struct us_socket_t *on_http_socket_end(struct us_socket_t *s) {
    return us_socket_close(config.SSL, s, 0, NULL);
}

/** @brief Handler for incoming data */
struct us_socket_t *on_http_socket_data(struct us_socket_t *s, char *data, int length) {
    SocketState *state = (SocketState *)us_socket_ext(config.SSL, s);
    if (state->is_upgraded) {
        state->outstanding_bytes -= length;
        if (state->outstanding_bytes == 0) {
            state->offset = us_socket_write(config.SSL, s, (char *)config.web_socket_request, config.web_socket_request_size, 0);
            state->outstanding_bytes = config.web_socket_request_response_size - 4;
            responses++;
        } else if (state->outstanding_bytes < 0) {
            printf("ERROR: outstanding bytes negative!\n");
            exit(0);
        }
    } else {
        if (length >= 4 && memcmp(data + length - 4, "\r\n\r\n", 4) == 0) {
            printf("Response: %.*s\n", length, data);
            state->offset = us_socket_write(config.SSL, s, (char *)config.web_socket_request, config.web_socket_request_size, 0);
            state->outstanding_bytes = config.web_socket_request_response_size - 4;
            state->is_upgraded = 1;
        }
    }
    return s;
}

/** @brief Handler for socket opening */
struct us_socket_t *on_http_socket_open(struct us_socket_t *s, int is_client, char *ip, int ip_length) {
    SocketState *state = (SocketState *)us_socket_ext(config.SSL, s);
    state->offset = 0;
    state->is_upgraded = 0;
    state->upgrade_offset = us_socket_write(config.SSL, s, config.upgrade_request, config.upgrade_request_length, 0);
    if (state->upgrade_offset == config.upgrade_request_length) {
        next_connection(s);
    }
    return s;
}

/** @brief Handler for socket timeout */
struct us_socket_t *on_http_socket_timeout(struct us_socket_t *s) {
    printf("Msg/sec: %f\n", ((float)responses) / LIBUS_TIMEOUT_GRANULARITY);
    responses = 0;
    us_socket_timeout(config.SSL, s, LIBUS_TIMEOUT_GRANULARITY);
    return s;
}

// Empty loop callbacks required by libusockets
void on_wakeup(struct us_loop_t *loop) {}
void on_pre(struct us_loop_t *loop) {}
void on_post(struct us_loop_t *loop) {}

/**
 * @brief Main entry point for the WebSocket benchmark
 * @param argc Number of command-line arguments
 * @param argv Array of command-line arguments
 * @return Exit status
 */
int main(int argc, char **argv) {
    if (argc != 6 && argc != 7) {
        printf("Usage: connections host port ssl deflate [payload_size_bytes]\n");
        return 0;
    }

    // Initialize configuration
    config.host = malloc(strlen(argv[2]) + 1);
    memcpy(config.host, argv[2], strlen(argv[2]) + 1);
    config.port = atoi(argv[3]);
    connections = atoi(argv[1]);
    config.SSL = atoi(argv[4]);
    config.deflate = atoi(argv[5]);

    // Set up upgrade request and WebSocket request based on deflate option
    if (config.deflate) {
        config.upgrade_request = request_deflate;
        config.upgrade_request_length = sizeof(request_deflate) - 1;
        config.payload_size = (argc == 7) ? atoi(argv[6]) : 5;
        init_deflated_request(config.payload_size, &config);
        printf("Using message size of %d bytes compressed down to %d bytes\n", config.payload_size, config.web_socket_request_size);
    } else {
        config.upgrade_request = request_text;
        config.upgrade_request_length = sizeof(request_text) - 1;
        if (argc == 7) {
            config.payload_size = atoi(argv[6]);
        } else {
            config.payload_size = 20;
        }
        init_text_request(config.payload_size, &config);
        printf("Using message size of %d bytes\n", config.payload_size);
    }

    // Create and run the event loop
    struct us_loop_t *loop = us_create_loop(0, on_wakeup, on_pre, on_post, 0);
    struct us_socket_context_options_t options = {0};
    struct us_socket_context_t *http_context = us_create_socket_context(config.SSL, loop, 0, options);

    // Register event handlers
    us_socket_context_on_open(config.SSL, http_context, on_http_socket_open);
    us_socket_context_on_data(config.SSL, http_context, on_http_socket_data);
    us_socket_context_on_writable(config.SSL, http_context, on_http_socket_writable);
    us_socket_context_on_close(config.SSL, http_context, on_http_socket_close);
    us_socket_context_on_timeout(config.SSL, http_context, on_http_socket_timeout);
    us_socket_context_on_end(config.SSL, http_context, on_http_socket_end);

    // Start the first connection
    us_socket_context_connect(config.SSL, http_context, config.host, config.port, NULL, 0, sizeof(SocketState));
    us_loop_run(loop);

    // Cleanup
    free(config.host);
    free(config.web_socket_request);
    us_socket_context_free(config.SSL, http_context);
    us_loop_free(loop);

    return 0;
}
