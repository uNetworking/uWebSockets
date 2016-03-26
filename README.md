# µWebSockets
`µWS` is one of the most lightweight, efficient & scalable WebSocket server implementations available. It features an easy-to-use, fully async object-oriented interface and scales to millions of connections using only a fraction of memory compared to the competition. License is zlib/libpng (very permissive & suites commercial applications).

* Passes Autobahn chapters 1 - 4 (work in progress)
* Planned SSL/TLS support (work in progress)

Project is a work in progress, initial commit was **21 March, 2016**. I'm planning to release something in a month or less.

## Benchmarks table
Implementation | Memory scaling | Short message throughput | Huge message throughput
--- | --- | --- | ---
libwebsockets | µWS is **10x** better | µWS is **2x** better | µWS is equal or slightly better
ws | µWS is **70x** better | µWS is **10x** better | µWS is **3x** better
WebSocket++ | µWS is **70x** better | µWS is equal or slightly better | µWS is **3x** better

## Overview
```c++
uWS::Server server(3000, ...);

server.onConnection([](uWS::Socket socket) {

});

server.onFragment([](uWS::Socket socket, const char *fragment, size_t length, uWS::OpCode opCode, , size_t remainingBytes) {

});

server.onDisconnection([](uWS::Socket socket) {

});
```

## Dependencies
* Unix (Linux, OS X)
* libuv
* OpenSSL
