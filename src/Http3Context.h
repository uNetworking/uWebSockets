namespace uWS {
    struct Http3Context {
        static Http3Context *create(us_loop_t *loop, us_socket_context_options_t options) {
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