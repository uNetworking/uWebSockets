# µWebSockets
`µWS` is a minimal WebSocket server library with focus on memory usage and performance. Reaching one million connections using only 250 mb of memory, while still being very competitive (read: among the best) in echo performance and data throughput.

Implementation | Memory scaling | Short message throughput | Huge message throughput
--- | --- | --- | ---
libwebsockets | µWS is **10x** better | µWS is **2x** better | µWS is equal or slightly better
ws | µWS is **70x** better | µWS is **10x** better | µWS is **3x** better
WebSocket++ | µWS is **70x** better | µWS is equal or slightly better | µWS is **3x** better

I do plan to support WSS (SSL/TLS) via OpenSSL

License is zlib/libpng (very permissive).

## Overview
```c++
uWS::Server server(3000, ...);

server.onConnection([](uWS::Socket socket) {

});

server.onFragment([](uWS::Socket socket, const char *fragment, size_t length, size_t remainingBytes) {

});

server.onDisconnection([](uWS::Socket socket) {

});
```

## Dependencies
* Unix (Linux, OS X)
* libuv
* OpenSSL
