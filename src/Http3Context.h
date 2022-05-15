
extern "C" {
#include "quic.h"
}

#include "Http3ContextData.h"
#include "Http3ResponseData.h"

/* Let's just have this one here for now */
us_quic_socket_context_t *context;

/* This would be a request */
void on_stream_headers(us_quic_stream_t *s) {

    Http3ContextData *contextData = (Http3ContextData *) us_quic_socket_context_ext(us_quic_socket_context(us_quic_stream_socket(s)));

    contextData->router.route();


}

/* And this would be the body of the request */
void on_stream_data(us_quic_stream_t *s, char *data, int length) {

    // Http3ResponseData *responseData = us_quic_stream_ext(s);
    // responseData->onData(data, length);

    printf("Body length is: %d\n", length);
}

void on_stream_writable(us_quic_stream_t *s) {
    // Http3ResponseData *responseData = us_quic_stream_ext(s);
    // responseData->onWritable();
}

void on_stream_close(us_quic_stream_t *s) {

    // Http3ResponseData *responseData = us_quic_stream_ext(s);
    // responseData->onAborted();

    //printf("Stream closed\n");

    // inplace destruct Http3ResponseData here
}

/* On new connection */
void on_open(us_quic_socket_t *s, int is_client) {
    printf("Connection established!\n");
}

/* On new stream */
void on_stream_open(us_quic_stream_t *s, int is_client) {
    //printf("Stream opened!\n");

    // inplace initialize Http3ResponseData here
}

void on_close(us_quic_socket_t *s) {
    printf("Disconnected!\n");
}

namespace uWS {
    struct Http3Context {
        static Http3Context *create(us_loop_t *loop, us_quic_socket_context_options_t options) {
            //return nullptr;

            printf("Creating context now\n");

            /* Create quic socket context (assumes h3 for now) */
            context = us_create_quic_socket_context(loop, options); // sizeof(Http3ContextData)

            /* Specify application callbacks */
            us_quic_socket_context_on_stream_data(context, on_stream_data);
            us_quic_socket_context_on_stream_open(context, on_stream_open);
            us_quic_socket_context_on_stream_close(context, on_stream_close);
            us_quic_socket_context_on_stream_writable(context, on_stream_writable);
            us_quic_socket_context_on_stream_headers(context, on_stream_headers);
            us_quic_socket_context_on_open(context, on_open);
            us_quic_socket_context_on_close(context, on_close);

            /* The listening socket is the actual UDP socket used */
            us_quic_listen_socket_t *listen_socket = us_quic_socket_context_listen(context, "::1", 9004); // sizeof(Http3ResponseData)


            return nullptr;

            // call init here after setting the ext to Http3ContextData
        }

        void init() {
            // set all callbacks here
        }

        void onHttp() {
            // modifies the router we own as part of Http3ContextData, used in callbacks set in init
        }
    };
}