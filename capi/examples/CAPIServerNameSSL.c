#include "../libuwebsockets.h"
#include <stdio.h>

uws_listen_socket_t *globalListenSocket;
uws_ssl_app_t *app;
void get_handler(uws_ssl_res_t *res, uws_req_t *req)
{
    uws_ssl_res_end(res, "Hello CAPI!", true);
}

void exit_handler(uws_ssl_res_t *res, uws_req_t *req)
{
	uws_ssl_res_end(res, "Shutting down!", true);
    /* We use this to check graceful closedown */
    uws_socket_close(globalListenSocket, false);
}

void missing_server_name_handler(const char *hostname){
	printf("We are missing server name: <%s>\n", hostname);
	
	/* Assume it is localhost, so add it */
	uws_ssl_add_server_name(app, "localhost");
}

void listen_handler(uws_listen_socket_t *listen_socket, uws_app_listen_config_t config)
{
    if (listen_socket){
        printf("Listening on port http://localhost:%d\n", config.port);
		globalListenSocket = listen_socket;
    }else{
		printf("Failed to listen on port http://localhost:%d\n", config.port);
	}
	
}

int main()
{
  	/* Overly simple hello world app (SNI)*/

    app = uws_create_ssl_app((uws_socket_context_options_t){
        /* There are example certificates in uWebSockets.js repo */
	    .key_file_name = "../misc/key.pem",
	    .cert_file_name = "../misc/cert.pem",
	    .passphrase = "1234"
    });
    uws_ssl_missing_server_name(app, missing_server_name_handler);
	uws_ssl_app_get(app, "/*", get_handler);
	uws_ssl_app_get(app, "/exit", exit_handler);
    uws_ssl_app_listen(app, 3000, listen_handler);
    
	/* Let's add a wildcard SNI to begin with */
	uws_ssl_add_server_name(app, "*.google.*");

	uws_ssl_app_run(app);
}