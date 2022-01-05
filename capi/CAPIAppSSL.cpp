#include "App.h"
#include "libuwebsockets.h"
#include <string_view>
#include <malloc.h>
static char *toNullTermineted(std::string_view text)
{
    char *dst = (char *)calloc(text.length() + 1, sizeof(char));
    strncat(dst, text.data(), text.length());
    return dst;
}
extern "C"
{

    uws_ssl_app_t *uws_create_ssl_app(uws_socket_context_options_t options)
    {
        uWS::SocketContextOptions sco;
        sco.ca_file_name = options.ca_file_name;
        sco.cert_file_name = options.cert_file_name;
        sco.dh_params_file_name = options.dh_params_file_name;
        sco.key_file_name = options.key_file_name;
        sco.passphrase = options.passphrase;
        sco.ssl_prefer_low_memory_usage = options.ssl_prefer_low_memory_usage;

        return (uws_ssl_app_t *)new uWS::SSLApp();
    }

    void uws_ssl_app_get(uws_ssl_app_t *app, const char *pattern, uws_ssl_method_handler handler)
    {
        uWS::SSLApp *uwsApp = (uWS::SSLApp *)app;
        uwsApp->get(pattern, [handler](auto *res, auto *req)
                    { handler((uws_ssl_res_t *)res, (uws_req_t *)req); });
    }
    void uws_ssl_app_post(uws_ssl_app_t *app, const char *pattern, uws_ssl_method_handler handler)
    {
        uWS::SSLApp *uwsApp = (uWS::SSLApp *)app;
        uwsApp->post(pattern, [handler](auto *res, auto *req)
                     { handler((uws_ssl_res_t *)res, (uws_req_t *)req); });
    }
    void uws_ssl_app_options(uws_ssl_app_t *app, const char *pattern, uws_ssl_method_handler handler)
    {
        uWS::SSLApp *uwsApp = (uWS::SSLApp *)app;
        uwsApp->options(pattern, [handler](auto *res, auto *req)
                        { handler((uws_ssl_res_t *)res, (uws_req_t *)req); });
    }
    void uws_ssl_app_delete(uws_ssl_app_t *app, const char *pattern, uws_ssl_method_handler handler)
    {
        uWS::SSLApp *uwsApp = (uWS::SSLApp *)app;
        uwsApp->del(pattern, [handler](auto *res, auto *req)
                    { handler((uws_ssl_res_t *)res, (uws_req_t *)req); });
    }
    void uws_ssl_app_patch(uws_ssl_app_t *app, const char *pattern, uws_ssl_method_handler handler)
    {
        uWS::SSLApp *uwsApp = (uWS::SSLApp *)app;
        uwsApp->patch(pattern, [handler](auto *res, auto *req)
                      { handler((uws_ssl_res_t *)res, (uws_req_t *)req); });
    }
    void uws_ssl_app_put(uws_ssl_app_t *app, const char *pattern, uws_ssl_method_handler handler)
    {
        uWS::SSLApp *uwsApp = (uWS::SSLApp *)app;
        uwsApp->put(pattern, [handler](auto *res, auto *req)
                    { handler((uws_ssl_res_t *)res, (uws_req_t *)req); });
    }
    void uws_ssl_app_head(uws_ssl_app_t *app, const char *pattern, uws_ssl_method_handler handler)
    {
        uWS::SSLApp *uwsApp = (uWS::SSLApp *)app;
        uwsApp->head(pattern, [handler](auto *res, auto *req)
                     { handler((uws_ssl_res_t *)res, (uws_req_t *)req); });
    }
    void uws_ssl_app_connect(uws_ssl_app_t *app, const char *pattern, uws_ssl_method_handler handler)
    {
        uWS::SSLApp *uwsApp = (uWS::SSLApp *)app;
        uwsApp->connect(pattern, [handler](auto *res, auto *req)
                        { handler((uws_ssl_res_t *)res, (uws_req_t *)req); });
    }
    void uws_ssl_app_trace(uws_ssl_app_t *app, const char *pattern, uws_ssl_method_handler handler)
    {
        uWS::SSLApp *uwsApp = (uWS::SSLApp *)app;
        uwsApp->trace(pattern, [handler](auto *res, auto *req)
                      { handler((uws_ssl_res_t *)res, (uws_req_t *)req); });
    }
    void uws_ssl_app_any(uws_ssl_app_t *app, const char *pattern, uws_ssl_method_handler handler)
    {
        uWS::SSLApp *uwsApp = (uWS::SSLApp *)app;
        uwsApp->any(pattern, [handler](auto *res, auto *req)
                    { handler((uws_ssl_res_t *)res, (uws_req_t *)req); });
    }

    void uws_ssl_app_run(uws_ssl_app_t *app)
    {
        uWS::SSLApp *uwsApp = (uWS::SSLApp *)app;
        uwsApp->run();
    }

    void uws_ssl_app_listen(uws_ssl_app_t *app, int port, uws_listen_handler handler)
    {
        uWS::SSLApp *uwsApp = (uWS::SSLApp *)app;
        uws_app_listen_config_t config;
        config.port = port;
        config.host = nullptr;
        config.options = 0;
        uwsApp->listen(port, [handler, config](struct us_listen_socket_t *listen_socket)
                       { handler((uws_listen_socket_t *)listen_socket, config); });
    }

    void uws_ssl_app_listen_with_config(uws_ssl_app_t *app, uws_app_listen_config_t config, uws_listen_handler handler)
    {
        uWS::SSLApp *uwsApp = (uWS::SSLApp *)app;
        uwsApp->listen(config.host, config.port, config.options, [handler, config](struct us_listen_socket_t *listen_socket)
                       { handler((uws_listen_socket_t *)listen_socket, config); });
    }

    void uws_ssl_app_destroy(uws_ssl_app_t *app)
    {
        uWS::SSLApp *uwsApp = (uWS::SSLApp *)app;
        delete uwsApp;
    }

    bool uws_ssl_constructor_failed(uws_ssl_app_t *app)
    {
        uWS::SSLApp *uwsApp = (uWS::SSLApp *)app;
        if (!uwsApp)
            return true;
        return uwsApp->constructorFailed();
    }

    unsigned int uws_ssl_num_subscribers(uws_ssl_app_t *app, const char *topic)
    {
        uWS::SSLApp *uwsApp = (uWS::SSLApp *)app;
        return uwsApp->numSubscribers(topic);
    }
    bool uws_ssl_publish(uws_ssl_app_t *app, const char *topic, const char *message, uws_opcode_t opcode, bool compress)
    {
        uWS::SSLApp *uwsApp = (uWS::SSLApp *)app;
        return uwsApp->publish(topic, message, (unsigned char)opcode, compress);
    }
    void *uws_ssl_get_native_handle(uws_ssl_app_t *app)
    {
        uWS::SSLApp *uwsApp = (uWS::SSLApp *)app;
        return uwsApp->getNativeHandle();
    }
    void uws_ssl_remove_server_name(uws_ssl_app_t *app, const char *hostname_pattern)
    {
        uWS::SSLApp *uwsApp = (uWS::SSLApp *)app;
        uwsApp->removeServerName(hostname_pattern);
    }
    void uws_ssl_add_server_name(uws_ssl_app_t *app, const char *hostname_pattern)
    {
        uWS::SSLApp *uwsApp = (uWS::SSLApp *)app;
        uwsApp->addServerName(hostname_pattern);
    }
    void uws_ssl_add_server_name_with_options(uws_ssl_app_t *app, const char *hostname_pattern, uws_socket_context_options_t options)
    {
        uWS::SSLApp *uwsApp = (uWS::SSLApp *)app;
        uWS::SocketContextOptions sco;
        sco.ca_file_name = options.ca_file_name;
        sco.cert_file_name = options.cert_file_name;
        sco.dh_params_file_name = options.dh_params_file_name;
        sco.key_file_name = options.key_file_name;
        sco.passphrase = options.passphrase;
        sco.ssl_prefer_low_memory_usage = options.ssl_prefer_low_memory_usage;

        uwsApp->addServerName(hostname_pattern, sco);
    }

    void uws_ssl_missing_server_name(uws_ssl_app_t *app, uws_missing_server_handler handler)
    {
        uWS::SSLApp *uwsApp = (uWS::SSLApp *)app;
        uwsApp->missingServerName(handler);
    }
    void uws_ssl_filter(uws_ssl_app_t *app, uws_ssl_filter_handler handler)
    {
        uWS::SSLApp *uwsApp = (uWS::SSLApp *)app;

        uwsApp->filter([handler](auto res, auto i)
                       { handler((uws_ssl_res_t *)res, i); });
    }

    void uws_ssl_ws(uws_ssl_app_t *app, const char *pattern, uws_ssl_socket_behavior_t behavior)
    {
        uWS::SSLApp *uwsApp = (uWS::SSLApp *)app;

        auto generic_handler = uWS::SSLApp::WebSocketBehavior<void *>{
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
                behavior.upgrade((uws_ssl_res_t *)res, (uws_req_t *)req, (uws_socket_context_t *)context);
            };
        if (behavior.open)
            generic_handler.open = [behavior](auto *ws)
            {
                behavior.open((uws_ssl_websocket_t *)ws);
            };
        if (behavior.message)
            generic_handler.message = [behavior](auto *ws, auto message, auto opcode)
            {
                behavior.message((uws_ssl_websocket_t *)ws, toNullTermineted(message), (uws_opcode_t)opcode);
            };
        if (behavior.drain)
            generic_handler.drain = [behavior](auto *ws)
            {
                behavior.drain((uws_ssl_websocket_t *)ws);
            };
        if (behavior.ping)
            generic_handler.ping = [behavior](auto *ws, auto message)
            {
                behavior.ping((uws_ssl_websocket_t *)ws, toNullTermineted(message));
            };
        if (behavior.pong)
            generic_handler.pong = [behavior](auto *ws, auto message)
            {
                behavior.pong((uws_ssl_websocket_t *)ws, toNullTermineted(message));
            };
        if (behavior.close)
            generic_handler.close = [behavior](auto *ws, int code, auto message)
            {
                behavior.close((uws_ssl_websocket_t *)ws, code, toNullTermineted(message));
            };
        uwsApp->ws<void *>(pattern, std::move(generic_handler));
    }

    void *uws_ssl_ws_get_user_data(uws_ssl_websocket_t *ws)
    {
        uWS::WebSocket<true, true, void *> *uws = (uWS::WebSocket<true, true, void *> *)ws;
        return *uws->getUserData();
    }

    void uws_ssl_ws_close(uws_ssl_websocket_t *ws)
    {
        uWS::WebSocket<true, true, void *> *uws = (uWS::WebSocket<true, true, void *> *)ws;
        uws->close();
    }

    uws_sendstatus_t uws_ssl_ws_send(uws_ssl_websocket_t *ws, const char *message, uws_opcode_t opcode)
    {
        uWS::WebSocket<true, true, void *> *uws = (uWS::WebSocket<true, true, void *> *)ws;
        return (uws_sendstatus_t)uws->send(message, (uWS::OpCode)(unsigned char)opcode);
    }

    uws_sendstatus_t uws_ssl_ws_send_with_options(uws_ssl_websocket_t *ws, const char *message, uws_opcode_t opcode, bool compress, bool fin)
    {
        uWS::WebSocket<true, true, void *> *uws = (uWS::WebSocket<true, true, void *> *)ws;
        return (uws_sendstatus_t)uws->send(message, (uWS::OpCode)(unsigned char)opcode, compress, fin);
    }

    uws_sendstatus_t uws_ssl_ws_send_fragment(uws_ssl_websocket_t *ws, const char *message, bool compress)
    {
        uWS::WebSocket<true, true, void *> *uws = (uWS::WebSocket<true, true, void *> *)ws;
        return (uws_sendstatus_t)uws->sendFragment(message, compress);
    }
    uws_sendstatus_t uws_ssl_ws_send_first_fragment(uws_ssl_websocket_t *ws, const char *message, bool compress)
    {
        uWS::WebSocket<true, true, void *> *uws = (uWS::WebSocket<true, true, void *> *)ws;
        return (uws_sendstatus_t)uws->sendFirstFragment(message, uWS::OpCode::BINARY, compress);
    }
    uws_sendstatus_t uws_ssl_ws_send_first_fragment_with_opcode(uws_ssl_websocket_t *ws, const char *message, uws_opcode_t opcode, bool compress)
    {
        uWS::WebSocket<true, true, void *> *uws = (uWS::WebSocket<true, true, void *> *)ws;
        return (uws_sendstatus_t)uws->sendFirstFragment(message, (uWS::OpCode)(unsigned char)opcode, compress);
    }
    uws_sendstatus_t uws_ssl_ws_send_last_fragment(uws_ssl_websocket_t *ws, const char *message, bool compress)
    {
        uWS::WebSocket<true, true, void *> *uws = (uWS::WebSocket<true, true, void *> *)ws;
        return (uws_sendstatus_t)uws->sendLastFragment(message, compress);
    }

    void uws_ssl_ws_end(uws_ssl_websocket_t *ws, int code, const char *message)
    {
        uWS::WebSocket<true, true, void *> *uws = (uWS::WebSocket<true, true, void *> *)ws;
        uws->end(code, message);
    }

    void uws_ssl_ws_cork(uws_ssl_websocket_t *ws, void (*handler)())
    {
        uWS::WebSocket<true, true, void *> *uws = (uWS::WebSocket<true, true, void *> *)ws;
        if (handler)
        {
            uws->cork([handler]()
                      { handler(); });
        }
    }
    bool uws_ssl_ws_subscribe(uws_ssl_websocket_t *ws, const char *topic)
    {
        uWS::WebSocket<true, true, void *> *uws = (uWS::WebSocket<true, true, void *> *)ws;
        return uws->subscribe(topic);
    }
    bool uws_ssl_ws_unsubscribe(uws_ssl_websocket_t *ws, const char *topic)
    {
        uWS::WebSocket<true, true, void *> *uws = (uWS::WebSocket<true, true, void *> *)ws;
        return uws->unsubscribe(topic);
    }

    bool uws_ssl_ws_is_subscribed(uws_ssl_websocket_t *ws, const char *topic)
    {
        uWS::WebSocket<true, true, void *> *uws = (uWS::WebSocket<true, true, void *> *)ws;
        return uws->isSubscribed(topic);
    }
    void uws_ssl_ws_iterate_topics(uws_ssl_websocket_t *ws, void (*callback)(const char *topic))
    {
        uWS::WebSocket<true, true, void *> *uws = (uWS::WebSocket<true, true, void *> *)ws;
        if (callback)
        {
            uws->iterateTopics([callback](auto topic)
                               { callback(toNullTermineted(topic)); });
        }
    }

    bool uws_ssl_ws_publish(uws_ssl_websocket_t *ws, const char *topic, const char *message)
    {
        uWS::WebSocket<true, true, void *> *uws = (uWS::WebSocket<true, true, void *> *)ws;
        return uws->publish(topic, message);
    }

    bool uws_ssl_ws_publish_with_options(uws_ssl_websocket_t *ws, const char *topic, const char *message, uws_opcode_t opcode, bool compress)
    {
        uWS::WebSocket<true, true, void *> *uws = (uWS::WebSocket<true, true, void *> *)ws;
        return uws->publish(topic, message, (uWS::OpCode)(unsigned char)opcode, compress);
    }

    unsigned int uws_ssl_ws_get_buffered_amount(uws_ssl_websocket_t *ws)
    {
        uWS::WebSocket<true, true, void *> *uws = (uWS::WebSocket<true, true, void *> *)ws;
        return uws->getBufferedAmount();
    }

    char *uws_ssl_ws_get_remote_address(uws_ssl_websocket_t *ws)
    {
        uWS::WebSocket<true, true, void *> *uws = (uWS::WebSocket<true, true, void *> *)ws;
        return toNullTermineted(uws->getRemoteAddress());
    }

    char *uws_ssl_ws_get_remote_address_as_text(uws_ssl_websocket_t *ws)
    {
        uWS::WebSocket<true, true, void *> *uws = (uWS::WebSocket<true, true, void *> *)ws;
        return toNullTermineted(uws->getRemoteAddressAsText());
    }

    void uws_ssl_res_end(uws_ssl_res_t *res, const char *data, bool close_connection)
    {
        uWS::HttpResponse<true> *uwsRes = (uWS::HttpResponse<true> *)res;
        uwsRes->end(data, close_connection);
    }

    void uws_ssl_res_pause(uws_ssl_res_t *res)
    {
        uWS::HttpResponse<true> *uwsRes = (uWS::HttpResponse<true> *)res;
        uwsRes->pause();
    }

    void uws_ssl_res_resume(uws_ssl_res_t *res)
    {
        uWS::HttpResponse<true> *uwsRes = (uWS::HttpResponse<true> *)res;
        uwsRes->pause();
    }

    void uws_ssl_res_write_continue(uws_ssl_res_t *res)
    {
        uWS::HttpResponse<true> *uwsRes = (uWS::HttpResponse<true> *)res;
        uwsRes->writeContinue();
    }

    void uws_ssl_res_write_status(uws_ssl_res_t *res, const char *status)
    {
        uWS::HttpResponse<true> *uwsRes = (uWS::HttpResponse<true> *)res;
        uwsRes->writeStatus(status);
    }

    void uws_ssl_res_write_header(uws_ssl_res_t *res, const char *key, const char *value)
    {
        uWS::HttpResponse<true> *uwsRes = (uWS::HttpResponse<true> *)res;
        uwsRes->writeHeader(key, value);
    }
    void uws_ssl_res_write_header_int(uws_ssl_res_t *res, const char *key, uint64_t value)
    {
        uWS::HttpResponse<true> *uwsRes = (uWS::HttpResponse<true> *)res;
        uwsRes->writeHeader(key, value);
    }

    void uws_ssl_res_end_without_body(uws_ssl_res_t *res)
    {
        uWS::HttpResponse<true> *uwsRes = (uWS::HttpResponse<true> *)res;
        uwsRes->endWithoutBody();
    }

    bool uws_ssl_res_write(uws_ssl_res_t *res, const char *data)
    {
        uWS::HttpResponse<true> *uwsRes = (uWS::HttpResponse<true> *)res;
        return uwsRes->write(data);
    }
    uintmax_t uws_ssl_res_get_write_offset(uws_ssl_res_t *res)
    {
        uWS::HttpResponse<true> *uwsRes = (uWS::HttpResponse<true> *)res;
        return uwsRes->getWriteOffset();
    }

    bool uws_ssl_res_has_responded(uws_ssl_res_t *res)
    {
        uWS::HttpResponse<true> *uwsRes = (uWS::HttpResponse<true> *)res;
        return uwsRes->hasResponded();
    }

    void uws_ssl_res_on_writable(uws_ssl_res_t *res, bool (*handler)(uws_ssl_res_t *res, uintmax_t, void *opcional_data), void *opcional_data)
    {
        uWS::HttpResponse<true> *uwsRes = (uWS::HttpResponse<true> *)res;
        uwsRes->onWritable([handler, res, opcional_data](uintmax_t a)
                           { return handler(res, a, opcional_data); });
    }

    void uws_ssl_res_on_aborted(uws_ssl_res_t *res, void (*handler)(uws_ssl_res_t *res, void *opcional_data), void *opcional_data)
    {
        uWS::HttpResponse<true> *uwsRes = (uWS::HttpResponse<true> *)res;
        uwsRes->onAborted([handler, res, opcional_data]
                          { handler(res, opcional_data); });
    }

    void uws_ssl_res_on_data(uws_ssl_res_t *res, void (*handler)(uws_ssl_res_t *res, const char *chunk, bool is_end, void *opcional_data), void *opcional_data)
    {
        uWS::HttpResponse<true> *uwsRes = (uWS::HttpResponse<true> *)res;
        uwsRes->onData([handler, res, opcional_data](auto chunk, bool is_end)
                       { handler(res, toNullTermineted(chunk), is_end, opcional_data); });
    }

    void uws_ssl_res_upgrade(uws_ssl_res_t *res, void *data, const char *sec_web_socket_key, const char *sec_web_socket_protocol, const char *sec_web_socket_extensions, uws_socket_context_t *ws)
    {
        uWS::HttpResponse<true> *uwsRes = (uWS::HttpResponse<true> *)res;

        uwsRes->template upgrade<void *>(data ? std::move(data) : NULL,
                                         sec_web_socket_key,
                                         sec_web_socket_protocol,
                                         sec_web_socket_extensions,
                                         (struct us_socket_context_t *)ws);
    }
}