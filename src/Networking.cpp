#include "Networking.h"

namespace uS {

namespace TLS {

Context createContext(std::string certChainFileName, std::string keyFileName, std::string keyFilePassword)
{
    auto *mbed_context = new mbedtls_ssl_context;
    mbedtls_ssl_init(mbed_context);

    auto *mbed_config = new mbedtls_ssl_config;
    mbedtls_ssl_config_init(mbed_config);

    Context context(mbed_context);
    if (!context.context) {
        return nullptr;
    }

    auto *mbed_x509 = new mbedtls_x509_crt;
    mbedtls_x509_crt_init(mbed_x509);
    int ret = mbedtls_x509_crt_parse_file(mbed_x509, certChainFileName.c_str());
    if (ret != 0)
      return nullptr;

    auto *mbed_pk = new mbedtls_pk_context;
    mbedtls_pk_init(mbed_pk);

    ret = mbedtls_pk_parse_keyfile(mbed_pk, keyFileName.c_str(), keyFilePassword.c_str());
    if (ret != 0)
      return nullptr;

    mbedtls_ssl_conf_own_cert(mbed_config, mbed_x509, mbed_pk);

    return context;
}

}

#ifndef _WIN32
struct Init {
    Init() {signal(SIGPIPE, SIG_IGN);}
} init;
#endif

#ifdef _WIN32
#pragma comment(lib, "Ws2_32.lib")

struct WindowsInit {
    WSADATA wsaData;
    WindowsInit() {WSAStartup(MAKEWORD(2, 2), &wsaData);}
    ~WindowsInit() {WSACleanup();}
} windowsInit;

#endif

}
