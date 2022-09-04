extern "C" {
#include "quic.h"
}

#include "Http3ResponseData.h"

namespace uWS {

    /* Is a quic stream */
    struct Http3Response {

        void writeStatus() {

        }

        void writeHeader() {

        }

        void tryEnd() {

        }

        void write() {

        }

        void end(std::string_view data) {

            Http3ResponseData *responseData = (Http3ResponseData *) us_quic_stream_ext((us_quic_stream_t *) this);

            // if not already written status then write status

            /* Write headers */
            us_quic_socket_context_set_header(nullptr, 0, (char *) ":status", 7, "200", 3);
            //us_quic_socket_context_set_header(context, 1, "content-length", 14, "11", 2);
            //us_quic_socket_context_set_header(context, 2, "content-type", 12, "text/html", 9);
            us_quic_socket_context_send_headers(nullptr, (us_quic_stream_t *) this, 1, 1);

            /* Write body and shutdown (unknown if content-length must be present?) */
            int written = us_quic_stream_write((us_quic_stream_t *) this, (char *) data.data(), data.length());

            printf("Wrote %d bytes out of %ld\n", written, data.length());

            /* Buffer up remains */
            if (written != data.length()) {
                responseData->buffer.clear();
                responseData->bufferOffset = written;
                responseData->buffer.append(data);
            } else {
                /* Every request has its own stream, so we conceptually serve requests like in HTTP 1.0 */
                us_quic_stream_shutdown((us_quic_stream_t *) this);
            }
        }

        /* Attach handler for aborted HTTP request */
        Http3Response *onAborted(MoveOnlyFunction<void()> &&handler) {
            Http3ResponseData *responseData = (Http3ResponseData *) us_quic_stream_ext((us_quic_stream_t *) this);

            responseData->onAborted = std::move(handler);
            return this;
        }

        /* Attach a read handler for data sent. Will be called with FIN set true if last segment. */
        void onData(MoveOnlyFunction<void(std::string_view, bool)> &&handler) {
            Http3ResponseData *responseData = (Http3ResponseData *) us_quic_stream_ext((us_quic_stream_t *) this);

            responseData->onData = std::move(handler);
        }
    };

}