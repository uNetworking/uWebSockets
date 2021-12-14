# Some notes to get this to build for centos7
git clone https://github.com/uNetworking/uWebSockets.git
cd uWebSockets/
git checkout feature/ios15_compression
git submodule update --init --recursive
sudo yum install libuv libuv-static libuv-devel openssl11 openssl11-devel devtoolset-9-gcc-c++ devtoolset-9-gdb
source /opt/rh/devtoolset-9/enable
#Maybe for clang but its not new enough for fuzzing - source /opt/rh/llvm-toolset-7/enable
make testcerts
WITH_OPENSSL=1 WITH_LIBUV=1 make AR=gcc-ar VERBOSE=1 all
 ./CompressionIOS15 --port 50444 --key ./key.pem --cert ./cert.pem --compression_dedicated

To be able to connect from IOS you need real certs with a cert with a matching Subject Alt Name.
I do this by reverse proxying via nginx server that has been setup with Lets Encrypt certs

# /etc/nginx/nginx.conf
user nginx;
worker_processes auto;
error_log /var/log/nginx/error.log;
pid /run/nginx.pid;

include /usr/share/nginx/modules/*.conf;

events {
    worker_connections 1024;
}


http {
    log_format  main  '$remote_addr - $remote_user [$time_local] "$request" '
                      '$status $body_bytes_sent "$http_referer" '
                      '"$http_user_agent" "$http_x_forwarded_for"';

    access_log  /var/log/nginx/access.log  main;

    sendfile            on;
    tcp_nopush          on;
    tcp_nodelay         on;
    keepalive_timeout   65;
    types_hash_max_size 2048;

    include             /etc/nginx/mime.types;
    default_type        application/octet-stream;

    # Load modular configuration files from the /etc/nginx/conf.d directory.
    # See http://nginx.org/en/docs/ngx_core_module.html#include
    # for more information.
    include /etc/nginx/conf.d/*.conf;
}

# /etc/nginx/config.d/ConnectionIOS15.conf
map $http_upgrade $connection_upgrade {
    default upgrade;
    ''      close;
}

server {
    listen 80;
    return 301 https://$host$request_uri;
}

server {
    listen 443 ssl;
    server_name uwebsockets.example.com;

    #Configure full path to https server certificate. To match server name above
    ssl_certificate     /etc/pki/tls/certs/uwebsockets.example.com.pem;
    ssl_certificate_key /etc/pki/tls/private/uwebsockets.example.com.pem;
    ssl_session_cache builtin:1000 shared:SSL:10m;
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers HIGH:!aNULL:!eNULL:!EXPORT:!CAMELLIA:!DES:!MD5:!PSK:!RC4;
    ssl_prefer_server_ciphers on;

    # Proxy to the horzionapi mini webservice
    location / {
        # https://stackoverflow.com/a/31403848
        # setsebool httpd_can_network_connect on -P
        proxy_pass https://127.0.0.1:50444/;
        proxy_http_version 1.1;
        proxy_headers_hash_bucket_size 128;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection $connection_upgrade;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        proxy_set_header Sec-WebSocket-Extensions $http_sec_websocket_extensions;
        #proxy_set_header Sec-WebSocket-Extensions ""; # <----------- This appears to squelch it.
        proxy_set_header Sec-WebSocket-Key $http_sec_websocket_key;
        proxy_set_header Sec-WebSocket-Version $http_sec_websocket_version;

        #proxy_ssl_verify off as there is currently using self signed certificates
        proxy_ssl_verify off;

        # If no communication on the socket within this time, nginx closes the
        # socket. Default time is 60 seconds.
        # Want to co-ordinate with uwebsocket and/or application level ping
        # interval, so that the frequency of regular pings hold the socket open.
        # proxy_read_timeout 90s;
    }
}
