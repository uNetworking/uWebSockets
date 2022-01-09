#include "libuwebsockets.h"
#include <string_view>
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
    uws_app_t *uws_create_app()
    {
        return (uws_app_t *)new uWS::App();
    }

    void uws_app_get(uws_app_t *app, const char *pattern, uws_method_handler handler, void *user_data)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uwsApp->get(pattern, [handler, user_data](auto *res, auto *req)
                    { handler((uws_res_t *)res, (uws_req_t *)req, user_data); });
    }
    void uws_app_post(uws_app_t *app, const char *pattern, uws_method_handler handler, void *user_data)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uwsApp->post(pattern, [handler, user_data](auto *res, auto *req)
                     { handler((uws_res_t *)res, (uws_req_t *)req, user_data); });
    }
    void uws_app_options(uws_app_t *app, const char *pattern, uws_method_handler handler, void *user_data)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uwsApp->options(pattern, [handler, user_data](auto *res, auto *req)
                        { handler((uws_res_t *)res, (uws_req_t *)req, user_data); });
    }
    void uws_app_delete(uws_app_t *app, const char *pattern, uws_method_handler handler, void *user_data)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uwsApp->del(pattern, [handler, user_data](auto *res, auto *req)
                    { handler((uws_res_t *)res, (uws_req_t *)req, user_data); });
    }
    void uws_app_patch(uws_app_t *app, const char *pattern, uws_method_handler handler, void *user_data)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uwsApp->patch(pattern, [handler, user_data](auto *res, auto *req)
                      { handler((uws_res_t *)res, (uws_req_t *)req, user_data); });
    }
    void uws_app_put(uws_app_t *app, const char *pattern, uws_method_handler handler, void *user_data)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uwsApp->put(pattern, [handler, user_data](auto *res, auto *req)
                    { handler((uws_res_t *)res, (uws_req_t *)req, user_data); });
    }
    void uws_app_head(uws_app_t *app, const char *pattern, uws_method_handler handler, void *user_data)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uwsApp->head(pattern, [handler, user_data](auto *res, auto *req)
                     { handler((uws_res_t *)res, (uws_req_t *)req, user_data); });
    }
    void uws_app_connect(uws_app_t *app, const char *pattern, uws_method_handler handler, void *user_data)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uwsApp->connect(pattern, [handler, user_data](auto *res, auto *req)
                        { handler((uws_res_t *)res, (uws_req_t *)req, user_data); });
    }
    void uws_app_trace(uws_app_t *app, const char *pattern, uws_method_handler handler, void *user_data)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uwsApp->trace(pattern, [handler, user_data](auto *res, auto *req)
                      { handler((uws_res_t *)res, (uws_req_t *)req, user_data); });
    }
    void uws_app_any(uws_app_t *app, const char *pattern, uws_method_handler handler, void *user_data)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uwsApp->any(pattern, [handler, user_data](auto *res, auto *req)
                    { handler((uws_res_t *)res, (uws_req_t *)req, user_data); });
    }

    void uws_app_run(uws_app_t *app)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uwsApp->run();
    }

    void uws_app_listen(uws_app_t *app, int port, uws_listen_handler handler, void *user_data)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uws_app_listen_config_t config;
        config.port = port;
        config.host = nullptr;
        config.options = 0;
        uwsApp->listen(port, [handler, config, user_data](struct us_listen_socket_t *listen_socket)
                       { handler((struct us_listen_socket_t *)listen_socket, config, user_data); });
    }

    void uws_app_listen_with_config(uws_app_t *app, uws_app_listen_config_t config, uws_listen_handler handler, void *user_data)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uwsApp->listen(config.host, config.port, config.options, [handler, config, user_data](struct us_listen_socket_t *listen_socket)
                       { handler((struct us_listen_socket_t *)listen_socket, config, user_data); });
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

    void uws_missing_server_name(uws_app_t *app, uws_missing_server_handler handler, void *user_data)
    {
        uWS::App *uwsApp = (uWS::App *)app;
        uwsApp->missingServerName([handler, user_data](auto hostname)
                                  { handler(hostname, user_data); });
    }
    void uws_filter(uws_app_t *app, uws_filter_handler handler, void *user_data)
    {
        uWS::App *uwsApp = (uWS::App *)app;

        uwsApp->filter([handler, user_data](auto res, auto i)
                       { handler((uws_res_t *)res, i, user_data); });
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
                behavior.message((uws_websocket_t *)ws, message.data(), message.length(), (uws_opcode_t)opcode);
            };
        if (behavior.drain)
            generic_handler.drain = [behavior](auto *ws)
            {
                behavior.drain((uws_websocket_t *)ws);
            };
        if (behavior.ping)
            generic_handler.ping = [behavior](auto *ws, auto message)
            {
                behavior.ping((uws_websocket_t *)ws, message.data(), message.length());
            };
        if (behavior.pong)
            generic_handler.pong = [behavior](auto *ws, auto message)
            {
                behavior.pong((uws_websocket_t *)ws, message.data(), message.length());
            };
        if (behavior.close)
            generic_handler.close = [behavior](auto *ws, int code, auto message)
            {
                behavior.close((uws_websocket_t *)ws, code, message.data(), message.length());
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

    void uws_ws_cork(uws_websocket_t *ws, void (*handler)(void *user_data), void *user_data)
    {
        uWS::WebSocket<false, true, void *> *uws = (uWS::WebSocket<false, true, void *> *)ws;
        if (handler)
        {
            uws->cork([handler, user_data]()
                      { handler(user_data); });
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
    void uws_ws_iterate_topics(uws_websocket_t *ws, void (*callback)(const char *topic, size_t length, void *user_data), void *user_data)
    {
        uWS::WebSocket<false, true, void *> *uws = (uWS::WebSocket<false, true, void *> *)ws;
        if (callback)
        {
            uws->iterateTopics([callback, user_data](auto topic)
                               { callback(topic.data(), topic.length(), user_data); });
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

    size_t uws_ws_get_remote_address(uws_websocket_t *ws, const char **dest)
    {
        uWS::WebSocket<false, true, void *> *uws = (uWS::WebSocket<false, true, void *> *)ws;

        std::string_view value = uws->getRemoteAddress();
        *dest = value.data();
        return value.length();
    }

    size_t uws_ws_get_remote_address_as_text(uws_websocket_t *ws, const char **dest)
    {
        uWS::WebSocket<false, true, void *> *uws = (uWS::WebSocket<false, true, void *> *)ws;

        std::string_view value = uws->getRemoteAddressAsText();
        *dest = value.data();
        return value.length();
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
                       { handler(res, chunk.data(), chunk.length(), is_end, opcional_data); });
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

    size_t uws_req_get_url(uws_req_t *res, const char **dest)
    {
        uWS::HttpRequest *uwsReq = (uWS::HttpRequest *)res;
        std::string_view value = uwsReq->getUrl();
        *dest = value.data();
        return value.length();
    }

    size_t uws_req_get_method(uws_req_t *res, const char **dest)
    {
        uWS::HttpRequest *uwsReq = (uWS::HttpRequest *)res;
        std::string_view value = uwsReq->getMethod();
        *dest = value.data();
        return value.length();
    }

    size_t uws_req_get_header(uws_req_t *res, const char *lower_case_header, size_t lower_case_header_length, const char **dest)
    {
        uWS::HttpRequest *uwsReq = (uWS::HttpRequest *)res;

        std::string_view value = uwsReq->getHeader(std::string_view(lower_case_header, lower_case_header_length));
        *dest = value.data();
        return value.length();
    }

    size_t uws_req_get_query(uws_req_t *res, const char *key, size_t key_length, const char **dest)
    {
        uWS::HttpRequest *uwsReq = (uWS::HttpRequest *)res;

        std::string_view value = uwsReq->getQuery(std::string_view(key, key_length));
        *dest = value.data();
        return value.length();
    }

    size_t uws_req_get_parameter(uws_req_t *res, unsigned short index, const char **dest)
    {
        uWS::HttpRequest *uwsReq = (uWS::HttpRequest *)res;
        std::string_view value = uwsReq->getParameter(index);
        *dest = value.data();
        return value.length();
    }

    void uws_res_upgrade(uws_res_t *res, void *data, const char *sec_web_socket_key, size_t sec_web_socket_key_length, const char *sec_web_socket_protocol, size_t sec_web_socket_protocol_length, const char *sec_web_socket_extensions, size_t sec_web_socket_extensions_length, uws_socket_context_t *ws)
    {
        uWS::HttpResponse<false> *uwsRes = (uWS::HttpResponse<false> *)res;

        uwsRes->template upgrade<void *>(data ? std::move(data) : NULL,
                                         std::string_view(sec_web_socket_key, sec_web_socket_key_length),
                                         std::string_view(sec_web_socket_protocol, sec_web_socket_protocol_length),
                                         std::string_view(sec_web_socket_extensions, sec_web_socket_extensions_length),
                                         (struct us_socket_context_t *)ws);
    }

    struct us_loop_t *uws_get_loop()
    {
        return (struct us_loop_t *)uWS::Loop::get();
    }
}