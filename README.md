<div align="center"><img src="logo.png"/></div>

µWS ("[micro](https://en.wikipedia.org/wiki/Micro-)WS") is simple and efficient message passing for the modern web.

* Read the [fancy pants details & user manual](https://todo.sometime).

#### Build optimized, standard web clients & servers in no time.
```c++
uWS::App a; // or uWS::SSLApp(options) for SSL

a.onGet("/", [](auto *s, auto *req, auto *args) {

    s->writeStatus("200 OK")->writeHeader("Hello", "World")->end(buffer);

}).onGet("/largefile", [](auto *s, auto *req, auto *args) {

    s->write([](int offset) {
        // stream chunks of the large file
    }, fileSize);

}).onPost("/upload/:filename", [](auto *s, auto *req, auto *args) {

    s->read([std::string filename = args[0]](int offset, auto chunk) {
        // stream chunks of data in to database with filename
    });

}).onWebSocket("/wsApi", [](auto *protocol) {

    protocol->onConnection([](auto *ws) {

        // store this websocket somewhere

    })->onMessage([](auto *ws, auto message, auto opCode) {

        ws->send(message, opCode);

    })->onDisconnection([]() {

        // remove this websocket

    });

}).listen("localhost", 3000, 0);
```
#### Deploy with confidence.
Feeling uncertain about your design? In need of professional help? Want to talk about the weather? I might have a few consulting hours for you and your business, send [me, the author](https://github.com/alexhultman) a mail and we'll figure out the rest.

#### Excel across the board.
<todo: insert new image here>
    
#### Pay what you want.
A free & open source ([Zlib](LICENSE)) project since 2016. Kindly sponsored by [BitMEX](https://bitmex.com), [Bitfinex](https://bitfinex.com) & [Coinbase](https://www.coinbase.com/) in 2018.

<div align="center"><img src="2018.png"/></div>

*Become a paying sponsor of the project to unlock support, issue reporting, roadmaps and to drop suggestions.*
