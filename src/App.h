#ifndef CONTEXT_H
#define CONTEXT_H

#include "libusockets.h"
#include "websocket/WebSocketApp.h"
#include "Loop.h"
#include <string>

namespace uWS {

class SSLOptions {
public:
    us_ssl_socket_context_options options = {};

    SSLOptions() {

    }

    SSLOptions &keyFileName(std::string fileName) {
        options.key_file_name = fileName.c_str();
        return *this;
    }

    SSLOptions &certFileName(std::string fileName) {
        options.cert_file_name = fileName.c_str();
        return *this;
    }

    SSLOptions &passphrase(std::string passphrase) {
        options.passphrase = passphrase.c_str();
        return *this;
    }
};

class App : public WebSocketApp<false> {

public:
    App() : WebSocketApp<false>(us_create_socket_context(defaultLoop.loop, sizeof(Data))) {

    }
};

class SSLApp : public WebSocketApp<true> {

public:
    SSLApp(SSLOptions &sslOptions) : WebSocketApp<true>(us_create_ssl_socket_context(defaultLoop.loop, sizeof(Data), sslOptions.options)) {

    }
};

}

#endif // CONTEXT_H
