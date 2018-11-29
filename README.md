<div align="center">
<img src="misc/logo.svg" height="180" />

*µWebSockets™ (it's "[micro](https://en.wikipedia.org/wiki/Micro-)") is simple, secure & standards compliant web I/O for the most demanding*<sup>[[1]](benchmarks)</sup> *of applications.*

• [Read more](misc/READMORE.md)

</div>

#### Tread lightly.
This is the **completely broken development branch** of v0.15. Track progress & issues [here](https://github.com/uNetworking/issues/issues). Checkout [v0.14](https://github.com/uNetworking/uWebSockets/tree/v0.14) for something stable.

#### Express yourself briefly.
```c++
uWS::SSLApp({

    /* There are tons of SSL options */
    .cert_file_name = "cert.pem",
    .key_file_name = "key.pem"
    
}).get("/hello", [](auto *res, auto *req) {

    /* You can efficiently stream huge files too */
    res->writeHeader("Content-Type", "text/html; charset=utf-8")->end("Hello HTTP!");
    
}).ws<UserData>("/*", {

    /* Just a few of the available handlers */
    .open = [](auto *ws, auto *req) {
        ws->subscribe("buzzword weekly");
    },
    .message = [](auto *ws, std::string_view message, uWS::OpCode opCode) {
        ws->send(message, opCode);
    }
    
}).listen(9001, [](auto *token) {

    if (token) {
        std::cout << "Listening on port " << 9001 << std::endl;
    }
    
}).run();
```

#### Pay what you want.
A free & open source ([permissive](LICENSE)) project since 2016. Kindly sponsored by [BitMEX](https://bitmex.com), [Bitfinex](https://bitfinex.com) & [Coinbase](https://www.coinbase.com/) in 2018.

<div align="center"><img src="misc/2018.png"/></div>

*Become a paying sponsor to unlock support, issue reporting, roadmaps and to drop suggestions.*

#### Deploy like a boss.
Commercial support is available via a per-hourly consulting plan or as otherwise negotiated. If you're stuck, worried about design or just in need of help don't hesitate throwing [me, the author](https://github.com/alexhultman) a mail and we'll figure out what's best for both parties. I want your business to have a proper understanding of the problem before rushing in to one of the many pitfalls.

#### Excel across the board.
All that glitters is not gold. Especially so in a market driven by flashy logos, hype and pointless badges.

Http | WebSockets
--- | ---
![](misc/bigshot_lineup.png) | ![](misc/websocket_lineup.png)
