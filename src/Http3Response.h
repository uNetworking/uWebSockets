namespace uWS {

    /* Is a quic stream */
    struct Http3Response {

        void writeStatus() {

        }

        void end(std::string_view data) {

            // Http3ResponseData *responseData = us_quic_stream_ext(this);

            // if not already written status then write status

            /* Write headers */
            us_quic_socket_context_set_header(context, 0, ":status", 7, "200", 3);
            //us_quic_socket_context_set_header(context, 1, "content-length", 14, "11", 2);
            //us_quic_socket_context_set_header(context, 2, "content-type", 12, "text/html", 9);
            us_quic_socket_context_send_headers(context, this, 1, 1);

            /* Write body and shutdown (unknown if content-length must be present?) */
            us_quic_stream_write(this, "Hello quic!", 11);

            /* Every request has its own stream, so we conceptually serve requests like in HTTP 1.0 */
            us_quic_stream_shutdown(this); 
        }
    };

}