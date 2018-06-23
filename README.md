# v0.15 of µWebSockets

µWS ("[micro](https://en.wikipedia.org/wiki/Micro-)WS") is a modern C++17 implementation of WebSockets & HTTP. It focuses on performance & memory usage and has been shown to outperform Node.js v10 some 13x (8x with SSL) while still being just as easy to work with.

#### Build optimized WebSocket & HTTP servers & clients in no time.
```c++
uWS::App a; // or uWS::SSLApp(options) for SSL

a.get("/", [](auto *s, auto *req, auto *args) {

    s->writeStatus(200)->writeHeader("Hello", "World")->end(buffer, 512);

}).get("/largefile", [](auto *s, auto *req, auto *args) {

    s->write([](int offset) {
        // stream chunks of the large file
    }, fileSize);

}).post("/upload/:filename", [](auto *s, auto *req, auto *args) {

    s->read([std::string filename = args[0]](int offset, auto chunk) {
        // stream chunks of data in to database with filename
    });

}).websocket("/wsApi", [](auto *protocol) {

    protocol->onConnection([](auto *ws) {

        // store this websocket somewhere

    })->onMessage([](auto *ws, auto message, auto opCode) {

        ws->send(message, opCode);

    })->onDisconnection([]() {

        // remove this websocket

    });

}).listen("localhost", 3000, 0);
```
