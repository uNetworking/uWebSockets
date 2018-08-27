<div align="center">
<img src="logo.png" />
    
*µWS ("[micro](https://en.wikipedia.org/wiki/Micro-)WS") is simple and efficient messaging for the modern web.*

• [Fancy pants details & user manual](http://)

</div>

#### Time is of the essence.
```c++
uWS::App a; // or uWS::SSLApp(options) for SSL

a.onGet("/", [](auto *s, auto *req, auto *args) {

    s->writeStatus("200 OK")->writeHeader("Hello", "World")->end(buffer);
    
}).onGet("/largefile", [](auto *s, auto *req, auto *args) {

    s->write([](int offset) {
        return largeFile.substr(offset);
    }, fileSize);
    
}).onPost("/upload/:filename", [](auto *s, auto *req, auto *args) {

    s->read([std::ofstream fout = args[0]](int offset, auto chunk) {
        fout << chunk;
    });
    
}).onWebSocket<UserData>("/wsApi", [](auto *ws, auto *req, auto *args) {

    std::cout << "WebSocket connected to /wsApi" << std::endl;
    
}).onMessage([](auto *ws, auto message, auto opCode) {

    ws->send(message, opCode);
    
}).onClose([](auto *ws, int code, auto message) {

    std::cout << "WebSocket disconnected from /wsApi" << std::endl;
    
}).listen("localhost", 3000, 0);
```
#### In more than one way.
![](https://github.com/uNetworking/uWebSockets/raw/master/misc/images/overview.png)

#### Pay what you want.
A free & open source ([Zlib](LICENSE)) project since 2016. Kindly sponsored by [BitMEX](https://bitmex.com), [Bitfinex](https://bitfinex.com) & [Coinbase](https://www.coinbase.com/) in 2018.

<div align="center"><img src="2018.png"/></div>

*Become a paying sponsor to unlock support, issue reporting, roadmaps and to drop suggestions.*

#### Stay confident.
If you're stuck, worried about design or just need help I might have a few consulting hours for you and your business. Throw [me, the author](https://github.com/alexhultman) a mail and we'll figure out the rest.
