# µWebSockets
`µWS` is a minimal WebSocket server library with focus on memory usage and performance. Reaching one million connections using only 250 mb of memory, while still being very competitive (read: among the best) in echo performance.

I'm not going to implement SSL, permessage deflate, extensions, etc. This project is just about you getting a full-duplex communication stream between your server and some connected web browser with as little overhead as possible.

* If you want compression, compress your messages.
* If you want SSL, encrypt your messages.

"Web" in WebSockets is basically a prefix meaning "retarded". It would have been a lot easier if we had a standardized pure TCP socket for use in the web browser. If you are still reading this, welcome aboard!

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
