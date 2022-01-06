#include "../libuwebsockets.h"
#include <stdio.h>


void get_handler(uws_ssl_res_t *res, uws_req_t *req)
{
    uws_ssl_res_end(res, "Hello CAPI!", false);
}

void listen_handler(uws_listen_socket_t *listen_socket, uws_app_listen_config_t config)
{
    if (listen_socket)
    {
        printf("Listening on port http://localhost:%d now\n", config.port);
    }
}

int main()
{
  	/* Overly simple hello world app */
    uws_ssl_app_t *app = uws_create_ssl_app((uws_socket_context_options_t){
        /* There are example certificates in uWebSockets.js repo */
	    .key_file_name = "../misc/key.pem",
	    .cert_file_name = "../misc/cert.pem",
	    .passphrase = "1234"
    });
    uws_ssl_app_get(app, "/*", get_handler);
    uws_ssl_app_listen(app, 3000, listen_handler);
    uws_ssl_app_run(app);
}
