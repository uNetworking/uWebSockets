extern "C" {
#include "quic.h"
}

namespace uWS {

    /* Is a quic stream */
    struct Http3Response {

        void writeStatus() {

        }

        void end(std::string_view data) {

            // Http3ResponseData *responseData = us_quic_stream_ext(this);

            // if not already written status then write status

            /* Write headers */
            us_quic_socket_context_set_header(nullptr, 0, (char *) ":status", 7, "200", 3);
            //us_quic_socket_context_set_header(context, 1, "content-length", 14, "11", 2);
            //us_quic_socket_context_set_header(context, 2, "content-type", 12, "text/html", 9);
            us_quic_socket_context_send_headers(nullptr, (us_quic_stream_t *) this, 1, 1);

            /* Write body and shutdown (unknown if content-length must be present?) */
            us_quic_stream_write((us_quic_stream_t *) this, (char *) data.data(), data.length());

            /* Every request has its own stream, so we conceptually serve requests like in HTTP 1.0 */
            us_quic_stream_shutdown((us_quic_stream_t *) this); 
        }
    };

}