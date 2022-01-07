#include "libuwebsockets.h"
#include <string_view>
#include <malloc.h>
#include "App.h"

extern "C"
{

    // uws_app_t *uws_create_app(int ssl, struct us_socket_context_options_t options)
    // {
    //     if (ssl)
    //     {
    //         uWS::SocketContextOptions sco;
    //         sco.ca_file_name = options.ca_file_name;
    //         sco.cert_file_name = options.cert_file_name;
    //         sco.dh_params_file_name = options.dh_params_file_name;
    //         sco.key_file_name = options.key_file_name;
    //         sco.passphrase = options.passphrase;
    //         sco.ssl_prefer_low_memory_usage = options.ssl_prefer_low_memory_usage;
    //         return (uws_app_t *) new uWS::SSLApp(sco);
    //     }
    uws_app_t *uws_create_app(){
        return (uws_app_t *)new uWS::App();
    }

    void uws_app_get(uws_app_t *app, const char *pattern, uws_method_handler handler)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uwsApp->get(pattern, [handler](auto *res, auto *req)
                    { handler((uws_res_t *)res, (uws_req_t *)req); });
    }
    void uws_app_post(uws_app_t *app, const char *pattern, uws_method_handler handler)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uwsApp->post(pattern, [handler](auto *res, auto *req)
                     { handler((uws_res_t *)res, (uws_req_t *)req); });
    }
    void uws_app_options(uws_app_t *app, const char *pattern, uws_method_handler handler)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uwsApp->options(pattern, [handler](auto *res, auto *req)
                        { handler((uws_res_t *)res, (uws_req_t *)req); });
    }
    void uws_app_delete(uws_app_t *app, const char *pattern, uws_method_handler handler)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uwsApp->del(pattern, [handler](auto *res, auto *req)
                    { handler((uws_res_t *)res, (uws_req_t *)req); });
    }
    void uws_app_patch(uws_app_t *app, const char *pattern, uws_method_handler handler)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uwsApp->patch(pattern, [handler](auto *res, auto *req)
                      { handler((uws_res_t *)res, (uws_req_t *)req); });
    }
    void uws_app_put(uws_app_t *app, const char *pattern, uws_method_handler handler)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uwsApp->put(pattern, [handler](auto *res, auto *req)
                    { handler((uws_res_t *)res, (uws_req_t *)req); });
    }
    void uws_app_head(uws_app_t *app, const char *pattern, uws_method_handler handler)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uwsApp->head(pattern, [handler](auto *res, auto *req)
                     { handler((uws_res_t *)res, (uws_req_t *)req); });
    }
    void uws_app_connect(uws_app_t *app, const char *pattern, uws_method_handler handler)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uwsApp->connect(pattern, [handler](auto *res, auto *req)
                        { handler((uws_res_t *)res, (uws_req_t *)req); });
    }
    void uws_app_trace(uws_app_t *app, const char *pattern, uws_method_handler handler)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uwsApp->trace(pattern, [handler](auto *res, auto *req)
                      { handler((uws_res_t *)res, (uws_req_t *)req); });
    }
    void uws_app_any(uws_app_t *app, const char *pattern, uws_method_handler handler)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uwsApp->any(pattern, [handler](auto *res, auto *req)
                    { handler((uws_res_t *)res, (uws_req_t *)req); });
    }

    void uws_app_run(uws_app_t *app)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uwsApp->run();
    }

    void uws_app_listen(uws_app_t *app, int port, uws_listen_handler handler)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uws_app_listen_config_t config;
        config.port = port;
        config.host = nullptr;
        config.options = 0;
        uwsApp->listen(port, [handler, config](struct us_listen_socket_t *listen_socket)
                       { handler((struct us_listen_socket_t *)listen_socket, config); });
    }

    void uws_app_listen_with_config(uws_app_t *app, uws_app_listen_config_t config, uws_listen_handler handler)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uwsApp->listen(config.host, config.port, config.options, [handler, config](struct us_listen_socket_t *listen_socket)
                       { handler((struct us_listen_socket_t *)listen_socket, config); });
    }

    void uws_app_destroy(uws_app_t *app)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        delete uwsApp;
    }

    bool uws_constructor_failed(uws_app_t *app)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        if (!uwsApp)
            return true;
        return uwsApp->constructorFailed();
    }

    unsigned int uws_num_subscribers(uws_app_t *app, const char *topic)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        return uwsApp->numSubscribers(topic);
    }
    bool uws_publish(uws_app_t *app, const char *topic, size_t topic_length, const char *message, size_t message_length, uws_opcode_t opcode, bool compress)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        return uwsApp->publish(std::string_view(topic, topic_length), std::string_view(message, message_length), (unsigned char)opcode, compress);
    }
    void *uws_get_native_handle(uws_app_t *app)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        return uwsApp->getNativeHandle();
    }
    void uws_remove_server_name(uws_app_t *app, const char *hostname_pattern)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uwsApp->removeServerName(hostname_pattern);
    }
    void uws_add_server_name(uws_app_t *app, const char *hostname_pattern)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uwsApp->addServerName(hostname_pattern);
    }
    void uws_add_server_name_with_options(uws_app_t *app, const char *hostname_pattern, uws_socket_context_options_t options)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uWS::SocketContextOptions sco;
        sco.ca_file_name = options.ca_file_name;
        sco.cert_file_name = options.cert_file_name;
        sco.dh_params_file_name = options.dh_params_file_name;
        sco.key_file_name = options.key_file_name;
        sco.passphrase = options.passphrase;
        sco.ssl_prefer_low_memory_usage = options.ssl_prefer_low_memory_usage;

        uwsApp->addServerName(hostname_pattern, sco);
    }

    void uws_missing_server_name(uws_app_t *app, uws_missing_server_handler handler)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uwsApp->missingServerName(handler);
    }
    void uws_filter(uws_app_t *app, uws_filter_handler handler)
    {
        uWS::App *uwsApp = (uWS::App *)app;

        uwsApp->filter([handler](auto res, auto i)
                       { handler((uws_res_t *)res, i); });
    }

    void uws_ws(uws_app_t *app, const char *pattern, uws_socket_behavior_t behavior)
    {
        uWS::App *uwsApp = (uWS::App *)app;

        auto generic_handler = uWS::App::WebSocketBehavior<void *>{
            .compression = (uWS::CompressOptions)(uint64_t)behavior.compression,
            .maxPayloadLength = behavior.maxPayloadLength,
            .idleTimeout = behavior.idleTimeout,
            .maxBackpressure = behavior.maxBackpressure,
            .closeOnBackpressureLimit = behavior.closeOnBackpressureLimit,
            .resetIdleTimeoutOnSend = behavior.resetIdleTimeoutOnSend,
            .sendPingsAutomatically = behavior.sendPingsAutomatically,
            .maxLifetime = behavior.maxLifetime,
        };

        if (behavior.upgrade)
            generic_handler.upgrade = [behavior](auto *res, auto *req, auto *context)
            {
                behavior.upgrade((uws_res_t *)res, (uws_req_t *)req, (uws_socket_context_t *)context);
            };
        if (behavior.open)
            generic_handler.open = [behavior](auto *ws)
            {
                behavior.open((uws_websocket_t *)ws);
            };
        if (behavior.message)
            generic_handler.message = [behavior](auto *ws, auto message, auto opcode)
            {
                behavior.message((uws_websocket_t *)ws, std::string(message).c_str(), message.length(), (uws_opcode_t)opcode);
            };
        if (behavior.drain)
            generic_handler.drain = [behavior](auto *ws)
            {
                behavior.drain((uws_websocket_t *)ws);
            };
        if (behavior.ping)
            generic_handler.ping = [behavior](auto *ws, auto message)
            {
                behavior.ping((uws_websocket_t *)ws, std::string(message).c_str(), message.length());
            };
        if (behavior.pong)
            generic_handler.pong = [behavior](auto *ws, auto message)
            {
                behavior.pong((uws_websocket_t *)ws, std::string(message).c_str(), message.length());
            };
        if (behavior.close)
            generic_handler.close = [behavior](auto *ws, int code, auto message)
            {
                behavior.close((uws_websocket_t *)ws, code, std::string(message).c_str(), message.length());
            };
        uwsApp->ws<void *>(pattern, std::move(generic_handler));
    }

    void *uws_ws_get_user_data(uws_websocket_t *ws)
    {
        uWS::WebSocket<false, true, void *> *uws = (uWS::WebSocket<false, true, void *> *)ws;
        return *uws->getUserData();
    }

    void uws_ws_close(uws_websocket_t *ws)
    {
        uWS::WebSocket<false, true, void *> *uws = (uWS::WebSocket<false, true, void *> *)ws;
        uws->close();
    }

    uws_sendstatus_t uws_ws_send(uws_websocket_t *ws, const char *message, size_t length, uws_opcode_t opcode)
    {
        uWS::WebSocket<false, true, void *> *uws = (uWS::WebSocket<false, true, void *> *)ws;
        return (uws_sendstatus_t)uws->send(std::string_view(message, length), (uWS::OpCode)(unsigned char)opcode);
    }

    uws_sendstatus_t uws_ws_send_with_options(uws_websocket_t *ws, const char *message, size_t length, uws_opcode_t opcode, bool compress, bool fin)
    {
        uWS::WebSocket<false, true, void *> *uws = (uWS::WebSocket<false, true, void *> *)ws;
        return (uws_sendstatus_t)uws->send(std::string_view(message), (uWS::OpCode)(unsigned char)opcode, compress, fin);
    }

    uws_sendstatus_t uws_ws_send_fragment(uws_websocket_t *ws, const char *message, size_t length, bool compress)
    {
        uWS::WebSocket<false, true, void *> *uws = (uWS::WebSocket<false, true, void *> *)ws;
        return (uws_sendstatus_t)uws->sendFragment(std::string_view(message, length), compress);
    }
    uws_sendstatus_t uws_ws_send_first_fragment(uws_websocket_t *ws, const char *message, size_t length, bool compress)
    {
        uWS::WebSocket<false, true, void *> *uws = (uWS::WebSocket<false, true, void *> *)ws;
        return (uws_sendstatus_t)uws->sendFirstFragment(std::string_view(message), uWS::OpCode::BINARY, compress);
    }
    uws_sendstatus_t uws_ws_send_first_fragment_with_opcode(uws_websocket_t *ws, const char *message, size_t length, uws_opcode_t opcode, bool compress)
    {
        uWS::WebSocket<false, true, void *> *uws = (uWS::WebSocket<false, true, void *> *)ws;
        return (uws_sendstatus_t)uws->sendFirstFragment(std::string_view(message, length), (uWS::OpCode)(unsigned char)opcode, compress);
    }
    uws_sendstatus_t uws_ws_send_last_fragment(uws_websocket_t *ws, const char *message, size_t length, bool compress)
    {
        uWS::WebSocket<false, true, void *> *uws = (uWS::WebSocket<false, true, void *> *)ws;
        return (uws_sendstatus_t)uws->sendLastFragment(std::string_view(message, length), compress);
    }

    void uws_ws_end(uws_websocket_t *ws, int code, const char *message, size_t length)
    {
        uWS::WebSocket<false, true, void *> *uws = (uWS::WebSocket<false, true, void *> *)ws;
        uws->end(code, std::string_view(message, length));
    }

    void uws_ws_cork(uws_websocket_t *ws, void (*handler)())
    {
        uWS::WebSocket<false, true, void *> *uws = (uWS::WebSocket<false, true, void *> *)ws;
        if (handler)
        {
            uws->cork([handler]()
                      { handler(); });
        }
    }
    bool uws_ws_subscribe(uws_websocket_t *ws, const char *topic, size_t length)
    {
        uWS::WebSocket<false, true, void *> *uws = (uWS::WebSocket<false, true, void *> *)ws;
        return uws->subscribe(std::string_view(topic, length));
    }
    bool uws_ws_unsubscribe(uws_websocket_t *ws, const char *topic, size_t length)
    {
        uWS::WebSocket<false, true, void *> *uws = (uWS::WebSocket<false, true, void *> *)ws;
        return uws->unsubscribe(std::string_view(topic, length));
    }

    bool uws_ws_is_subscribed(uws_websocket_t *ws, const char *topic, size_t length)
    {
        uWS::WebSocket<false, true, void *> *uws = (uWS::WebSocket<false, true, void *> *)ws;
        return uws->isSubscribed(std::string_view(topic, length));
    }
    void uws_ws_iterate_topics(uws_websocket_t *ws, void (*callback)(const char *topic, size_t length))
    {
        uWS::WebSocket<false, true, void *> *uws = (uWS::WebSocket<false, true, void *> *)ws;
        if (callback)
        {
            uws->iterateTopics([callback](auto topic)
                               { callback(std::string(topic).c_str(), topic.length()); });
        }
    }

    bool uws_ws_publish(uws_websocket_t *ws, const char *topic, size_t topic_length, const char *message, size_t message_length)
    {
        uWS::WebSocket<false, true, void *> *uws = (uWS::WebSocket<false, true, void *> *)ws;
        return uws->publish(std::string_view(topic, topic_length), std::string_view(message, message_length));
    }

    bool uws_ws_publish_with_options(uws_websocket_t *ws, const char *topic, size_t topic_length, const char *message, size_t message_length, uws_opcode_t opcode, bool compress)
    {
        uWS::WebSocket<false, true, void *> *uws = (uWS::WebSocket<false, true, void *> *)ws;
        return uws->publish(std::string_view(topic, topic_length), std::string_view(message, message_length), (uWS::OpCode)(unsigned char)opcode, compress);
    }

    unsigned int uws_ws_get_buffered_amount(uws_websocket_t *ws)
    {
        uWS::WebSocket<false, true, void *> *uws = (uWS::WebSocket<false, true, void *> *)ws;
        return uws->getBufferedAmount();
    }

    const char *uws_ws_get_remote_address(uws_websocket_t *ws)
    {
        uWS::WebSocket<false, true, void *> *uws = (uWS::WebSocket<false, true, void *> *)ws;
        return std::string(uws->getRemoteAddress()).c_str();
    }

    const char *uws_ws_get_remote_address_as_text(uws_websocket_t *ws)
    {
        uWS::WebSocket<false, true, void *> *uws = (uWS::WebSocket<false, true, void *> *)ws;
        return std::string(uws->getRemoteAddressAsText()).c_str();
        ;
    }

    void uws_res_end(uws_res_t *res, const char *data, size_t length, bool close_connection)
    {
        uWS::HttpResponse<false> *uwsRes = (uWS::HttpResponse<false> *)res;
        uwsRes->end(std::string_view(data, length), close_connection);
    }

    void uws_res_pause(uws_res_t *res)
    {
        uWS::HttpResponse<false> *uwsRes = (uWS::HttpResponse<false> *)res;
        uwsRes->pause();
    }

    void uws_res_resume(uws_res_t *res)
    {
        uWS::HttpResponse<false> *uwsRes = (uWS::HttpResponse<false> *)res;
        uwsRes->pause();
    }

    void uws_res_write_continue(uws_res_t *res)
    {
        uWS::HttpResponse<false> *uwsRes = (uWS::HttpResponse<false> *)res;
        uwsRes->writeContinue();
    }

    void uws_res_write_status(uws_res_t *res, const char *status, size_t length)
    {
        uWS::HttpResponse<false> *uwsRes = (uWS::HttpResponse<false> *)res;
        uwsRes->writeStatus(std::string_view(status, length));
    }

    void uws_res_write_header(uws_res_t *res, const char *key, size_t key_length, const char *value, size_t value_length)
    {
        uWS::HttpResponse<false> *uwsRes = (uWS::HttpResponse<false> *)res;
        uwsRes->writeHeader(std::string_view(key, key_length), std::string_view(value, value_length));
    }
    void uws_res_write_header_int(uws_res_t *res, const char *key, size_t key_length, uint64_t value)
    {
        uWS::HttpResponse<false> *uwsRes = (uWS::HttpResponse<false> *)res;
        uwsRes->writeHeader(std::string_view(key, key_length), value);
    }

    void uws_res_end_without_body(uws_res_t *res)
    {
        uWS::HttpResponse<false> *uwsRes = (uWS::HttpResponse<false> *)res;
        uwsRes->endWithoutBody(0);
    }

    bool uws_res_write(uws_res_t *res, const char *data, size_t length)
    {
        uWS::HttpResponse<false> *uwsRes = (uWS::HttpResponse<false> *)res;
        return uwsRes->write(std::string_view(data));
    }
    uintmax_t uws_res_get_write_offset(uws_res_t *res)
    {
        uWS::HttpResponse<false> *uwsRes = (uWS::HttpResponse<false> *)res;
        return uwsRes->getWriteOffset();
    }

    bool uws_res_has_responded(uws_res_t *res)
    {
        uWS::HttpResponse<false> *uwsRes = (uWS::HttpResponse<false> *)res;
        return uwsRes->hasResponded();
    }

    void uws_res_on_writable(uws_res_t *res, bool (*handler)(uws_res_t *res, uintmax_t, void *opcional_data), void *opcional_data)
    {
        uWS::HttpResponse<false> *uwsRes = (uWS::HttpResponse<false> *)res;
        uwsRes->onWritable([handler, res, opcional_data](uintmax_t a)
                           { return handler(res, a, opcional_data); });
    }

    void uws_res_on_aborted(uws_res_t *res, void (*handler)(uws_res_t *res, void *opcional_data), void *opcional_data)
    {
        uWS::HttpResponse<false> *uwsRes = (uWS::HttpResponse<false> *)res;
        uwsRes->onAborted([handler, res, opcional_data]
                          { handler(res, opcional_data); });
    }

    void uws_res_on_data(uws_res_t *res, void (*handler)(uws_res_t *res, const char *chunk, size_t chunk_length, bool is_end, void *opcional_data), void *opcional_data)
    {
        uWS::HttpResponse<false> *uwsRes = (uWS::HttpResponse<false> *)res;
        uwsRes->onData([handler, res, opcional_data](auto chunk, bool is_end)
                       { handler(res, std::string(chunk).c_str(), chunk.length(), is_end, opcional_data); });
    }

    bool uws_req_is_ancient(uws_req_t *res)
    {
        uWS::HttpRequest *uwsReq = (uWS::HttpRequest *)res;
        return uwsReq->isAncient();
    }

    bool uws_req_get_yield(uws_req_t *res)
    {
        uWS::HttpRequest *uwsReq = (uWS::HttpRequest *)res;
        return uwsReq->getYield();
    }

    void uws_req_set_field(uws_req_t *res, bool yield)
    {
        uWS::HttpRequest *uwsReq = (uWS::HttpRequest *)res;
        return uwsReq->setYield(yield);
    }

    const char *uws_req_get_url(uws_req_t *res)
    {
        uWS::HttpRequest *uwsReq = (uWS::HttpRequest *)res;
        return std::string(uwsReq->getUrl()).c_str();
    }

    const char *uws_req_get_method(uws_req_t *res)
    {
        uWS::HttpRequest *uwsReq = (uWS::HttpRequest *)res;
        return std::string(uwsReq->getMethod()).c_str();
    }

    const char *uws_req_get_header(uws_req_t *res, const char *lower_case_header, size_t lower_case_header_length)
    {
        uWS::HttpRequest *uwsReq = (uWS::HttpRequest *)res;

        std::string_view header = uwsReq->getHeader(std::string_view(lower_case_header, lower_case_header_length));
        return std::string(header).c_str();
    }

    const char *uws_req_get_query(uws_req_t *res, const char *key, size_t key_length)
    {
        uWS::HttpRequest *uwsReq = (uWS::HttpRequest *)res;
        return std::string(uwsReq->getQuery(std::string_view(key, key_length))).c_str();
    }

    const char *uws_req_get_parameter(uws_req_t *res, unsigned short index)
    {
        uWS::HttpRequest *uwsReq = (uWS::HttpRequest *)res;
        return std::string(uwsReq->getParameter(index)).c_str();
    }
    // void uws_req_set_parameters(uws_req_t *res, int first, const char* second) {
    //     uWS::HttpRequest * uwsReq = (uWS::HttpRequest*)res;
    //     std::pair<int, std::string_view *> parameters;
    //     parameters.first = first;
    //     parameters.second = std::string_view(std::string(second));
    //     uwsReq->setParameters(parameters);
    // }

    void uws_res_upgrade(uws_res_t *res, void *data, const char *sec_web_socket_key, const char *sec_web_socket_protocol, const char *sec_web_socket_extensions, uws_socket_context_t *ws)
    {
        uWS::HttpResponse<false> *uwsRes = (uWS::HttpResponse<false> *)res;

        uwsRes->template upgrade<void *>(data ? std::move(data) : NULL,
                                         sec_web_socket_key,
                                         sec_web_socket_protocol,
                                         sec_web_socket_extensions,
                                         (struct us_socket_context_t *)ws);
    }

    void uws_timer_close(uws_timer_t *timer)
    {
        struct us_timer_t *t = (struct us_timer_t *)timer;
        struct timer_handler_data *data;
        memcpy(&data, us_timer_ext(t), sizeof(struct timer_handler_data *));
        free(data);
        us_timer_close(t);
    }

    uws_timer_t *uws_create_timer(int ms, int repeat_ms, void (*handler)(void *data), void *data)
    {
        struct us_loop_t *loop = (struct us_loop_t *)uWS::Loop::get();
        struct us_timer_t *delayTimer = us_create_timer(loop, 0, sizeof(void *));

        struct timer_handler_data
        {
            void *data;
            void (*handler)(void *data);
            bool repeat;
        };

        struct timer_handler_data *timer_data = (struct timer_handler_data *)malloc(sizeof(timer_handler_data));
        timer_data->data = data;
        timer_data->handler = handler;
        timer_data->repeat = repeat_ms > 0;
        memcpy(us_timer_ext(delayTimer), &timer_data, sizeof(struct timer_handler_data *));

        us_timer_set(
            delayTimer, [](struct us_timer_t *t)
            {
                /* We wrote the pointer to the timer's extension */
                struct timer_handler_data *data;
                memcpy(&data, us_timer_ext(t), sizeof(struct timer_handler_data *));

                data->handler(data->data);

                if (!data->repeat)
                {
                    free(data);
                    us_timer_close(t);
                }
            },
            ms, repeat_ms);

        return (uws_timer_t *)delayTimer;
    }
}