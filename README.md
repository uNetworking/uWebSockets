<div style="text-align: center"><img src ="logo.png" /></div>
`µWS` is one of the most lightweight, efficient & scalable WebSocket server implementations available. It features an easy-to-use, fully async object-oriented interface and scales to millions of connections using only a fraction of memory compared to the competition. License is zlib/libpng (very permissive & suits commercial applications).

* Planned SSL/TLS support (work in progress)
* Node.js bindings are planned and will target the `ws` interface.

Project is a work in progress, initial commit was **21 March, 2016**. I'm planning to release something in a month or less.

## Benchmarks table
Implementation | Memory scaling | Connection performance | Short message throughput | Huge message throughput
--- | --- | --- | --- | ---
libwebsockets master(1.7-1.8) | µWS is **14x** as lightweight | µWS is **equal** in performance | µWS is **3x** as performant | µWS is **equal** in performance
ws v1.0.1 + binary addons | µWS is **52x** as lightweight | µWS is **21x** as performant | µWS is **23x** as performant | µWS is **2x** as performant
WebSocket++ v0.7.0 | µWS is **63x** as lightweight | µWS is **5x** as performant | µWS is **2x** as performant | µWS is **3x** as performant

## Quality control
* Valgrind clean
* Autobahn chapters 1 - 9 [all pass](http://htmlpreview.github.io/?https://github.com/alexhultman/uWebSockets/blob/master/Autobahn/index.html).
* Small & efficient code base.

## Overview
```c++
int main()
{
    /* this is an echo server that properly passes every supported Autobahn test */
    uWS::Server server(3000);
    server.onConnection([](uWS::Socket socket) {
        cout << "[Connection] clients: " << ++connections << endl;
    });

    server.onMessage([](uWS::Socket socket, const char *message, size_t length, OpCode opCode) {
        socket.send((char *) message, length, opCode);
    });

    server.onDisconnection([](uWS::Socket socket) {
        cout << "[Disconnection] clients: " << --connections << endl;
    });

    server.run();
}
```

## Dependencies
* Unix (Linux, OS X)
* libuv
* OpenSSL
