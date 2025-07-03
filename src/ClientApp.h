#pragma once
#include "Loop.h"
#include "MoveOnlyFunction.h"
#include "WebSocketHandshake.h"
#include <string>
#include <string_view>
#include <vector>
#include <random>
#include <cstring>
#include <type_traits>

#include <iostream>

namespace uWS {

struct WebSocketClientBehavior {
    MoveOnlyFunction<void()> open;
    MoveOnlyFunction<void(std::string_view)> message;
    MoveOnlyFunction<void()> close;
};

class ClientApp {
    struct us_socket_context_t *context = nullptr;
    struct us_socket_t *socket = nullptr;
    std::string host;
    std::string path;
    int port = 80;
    WebSocketClientBehavior behavior;
    std::string recvBuffer;
    std::string secKey;
    bool handshakeDone = false;

    static std::string base64(const unsigned char *src, size_t len) {
        static const char *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        out.resize(((len + 2) / 3) * 4);
        size_t i, j;
        for (i = 0, j = 0; i + 2 < len; i += 3) {
            out[j++] = b64[(src[i] >> 2) & 63];
            out[j++] = b64[((src[i] & 3) << 4) | ((src[i + 1] & 0xf0) >> 4)];
            out[j++] = b64[((src[i + 1] & 0x0f) << 2) | ((src[i + 2] & 0xc0) >> 6)];
            out[j++] = b64[src[i + 2] & 63];
        }
        if (i < len) {
            out[j++] = b64[(src[i] >> 2) & 63];
            if (i + 1 < len) {
                out[j++] = b64[((src[i] & 3) << 4) | ((src[i + 1] & 0xf0) >> 4)];
                out[j++] = b64[(src[i + 1] & 0x0f) << 2];
                out[j++] = '=';
            } else {
                out[j++] = b64[(src[i] & 3) << 4];
                out[j++] = '=';
                out[j++] = '=';
            }
        }
        return out;
    }

    static ClientApp *get(struct us_socket_t *s) {
        return *(ClientApp **) us_socket_context_ext(0, us_socket_context(0, s));
    }

    static struct us_socket_t *onOpen(struct us_socket_t *s, int is_client, char *ip, int ip_length) {
        std::cout << "WebSocket client connected to (" << ip_length << "): " << std::string_view(ip, ip_length) << '\n' << std::endl;

        ClientApp *self = get(s);
        self->socket = s;
        unsigned char rnd[16];
        std::random_device rd;
        for (int i = 0; i < 16; i++) {
            rnd[i] = rd();
        }
        self->secKey = base64(rnd, 16);
        std::string req = "GET " + self->path + " HTTP/1.1\r\n";
        req += "Host: " + self->host + "\r\n";
        req += "Upgrade: websocket\r\nConnection: Upgrade\r\n";
        req += "Sec-WebSocket-Key: " + self->secKey + "\r\n";
        req += "Sec-WebSocket-Version: 13\r\n\r\n";

        std::cout << "Sending handshake request:\n" << req << '\n' << std::endl;

        us_socket_write(0, s, req.c_str(), (int) req.length(), 0);
        
        return s;
    }

    static struct us_socket_t *onData(struct us_socket_t *s, char *data, int length) {
        std::cout << "WebSocket client received data:\n" << std::string_view(data, length) << '\n' << std::endl;

        ClientApp *self = get(s);
        self->recvBuffer.append(data, length);
        if (!self->handshakeDone) {
            auto pos = self->recvBuffer.find("\r\n\r\n");
            if (pos != std::string::npos) {
                std::string headers = self->recvBuffer.substr(0, pos + 4);
                self->recvBuffer.erase(0, pos + 4);
                if (headers.find("101") != std::string::npos) {
                    char accept[28];
                    WebSocketHandshake::generate(self->secKey.c_str(), accept);
                    std::string acceptHdr = std::string("Sec-WebSocket-Accept: ") + std::string(accept, 28) + "\r\n";
                    if (headers.find(acceptHdr) != std::string::npos) {
                        self->handshakeDone = true;
                        if (self->behavior.open) self->behavior.open();
                    } else {
                        std::cout << "Não achei o accept header esperado (" << acceptHdr.size() << ") \"" << acceptHdr << "\", por isso encerrando..." << std::endl;
                        us_socket_close(0, s, 0, nullptr);
                    }
                } else {
                    std::cout << "Não achei o \"101\", por isso encerrando..." << std::endl;
                    us_socket_close(0, s, 0, nullptr);
                }
            }
            return s;
        }
        if (self->recvBuffer.size() >= 2) {
            unsigned char b0 = self->recvBuffer[0];
            unsigned char b1 = self->recvBuffer[1];
            bool fin = b0 & 0x80;
            unsigned char op = b0 & 0x0f;
            size_t offset = 2;
            uint64_t len64 = b1 & 0x7f;
            if (len64 == 126) {
                if (self->recvBuffer.size() < offset + 2) return s;
                len64 = ((unsigned char)self->recvBuffer[offset] << 8) | (unsigned char)self->recvBuffer[offset + 1];
                offset += 2;
            } else if (len64 == 127) {
                if (self->recvBuffer.size() < offset + 8) return s;
                len64 = 0;
                for (int i = 0; i < 8; i++) {
                    len64 = (len64 << 8) | (unsigned char)self->recvBuffer[offset + i];
                }
                offset += 8;
            }
            if (self->recvBuffer.size() < offset + len64) return s;
            std::string_view msg(self->recvBuffer.data() + offset, len64);
            if (op == 1 && self->behavior.message) {
                self->behavior.message(msg);
            }
            self->recvBuffer.erase(0, offset + len64);
        }
        return s;
    }

    static struct us_socket_t *onClose(struct us_socket_t *s, int code, void *reason) {
        std::cout << "WebSocket client closed connection with code: " << code << '\n' << std::endl;

        ClientApp *self = get(s);
        if (self->behavior.close) self->behavior.close();
        return s;
    }

public:
    template <class T, typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, ClientApp>>>
    ClientApp(T &&behavior) : behavior(std::forward<T>(behavior)) {}

    ClientApp &&connect(std::string url, std::string protocol = "") {
        if (url.rfind("ws://", 0) == 0) {
            url = url.substr(5);
        }
        size_t slash = url.find('/');
        std::string hostPort = url.substr(0, slash);
        path = (slash != std::string::npos) ? url.substr(slash) : "/";
        size_t colon = hostPort.find(':');
        if (colon != std::string::npos) {
            host = hostPort.substr(0, colon);
            port = std::stoi(hostPort.substr(colon + 1));
        } else {
            host = hostPort;
            port = 80;
        }

        struct us_socket_context_options_t options = {};
        context = us_create_socket_context(0, (us_loop_t *) Loop::get(), sizeof(ClientApp *), options);
        *(ClientApp **) us_socket_context_ext(0, context) = this;
        us_socket_context_on_open(0, context, onOpen);
        us_socket_context_on_data(0, context, onData);
        us_socket_context_on_close(0, context, onClose);
        us_socket_context_connect(0, context, host.c_str(), port, nullptr, 0, 0);
        return std::move(*this);
    }

    void run() {
        Loop::get()->run();
    }
};

}
