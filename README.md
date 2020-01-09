<div align="center">
<img src="misc/logo.svg" height="180" />

*µWebSockets™ (it's "[micro](https://en.wikipedia.org/wiki/Micro-)") is simple, secure*<sup>[[1]](fuzzing)</sup> *& standards compliant*<sup>[[2]](https://unetworking.github.io/uWebSockets.js/report.pdf)</sup> *web I/O for the most demanding*<sup>[[3]](benchmarks)</sup> *of applications.*

• [User manual](misc/READMORE.md) • [uSockets](https://github.com/uNetworking/uSockets) • [For Python](https://github.com/uNetworking/uWebSockets.py) • [For Node.js](https://github.com/uNetworking/uWebSockets.js)

*© 2016-2019, >39,632,272 downloads*

</div>

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
Don't miss the [user manual](https://github.com/uNetworking/uWebSockets/blob/master/misc/READMORE.md#user-manual), the [C++ examples](https://github.com/uNetworking/uWebSockets/tree/master/examples) or the [JavaScript examples](https://github.com/uNetworking/uWebSockets.js/tree/master/examples). JavaScript examples are very applicable to C++ developers, so go through them as well.

#### Pay what you want.
A free & open source ([permissive](LICENSE)) project since 2016. Kindly sponsored by [BitMEX](https://bitmex.com), [Bitfinex](https://bitfinex.com) & [Coinbase](https://www.coinbase.com/) in 2018 and/or 2019. Individual donations are always accepted via [PayPal](https://paypal.me/uwebsockets).

<div align="center"><img src="misc/2018.png"/></div>

*Code is provided as-is, do not expect or demand **free** consulting services, personal tutoring, advice or debugging.*

#### Deploy like a boss.
Commercial support is available via a per-hourly consulting plan or as otherwise negotiated. If you're stuck, worried about design or just in need of help don't hesitate throwing [me, the author](https://github.com/alexhultman) a mail and we'll figure out what's best for both parties. I want your business to have a proper understanding of the problem before rushing in to one of the many pitfalls.

#### Excel across the board.
All that glitters is not gold. Especially so in a market driven by flashy logos, hype and pointless badges.

Http | WebSockets
--- | ---
![](misc/bigshot_lineup.png) | ![](misc/websocket_lineup.png)

#### Keep it legal.
Intellectual property, all rights reserved.

*You are forbidden to use logos, product names, texts, names or otherwise perceived brand identity, of copyright holder, in any way that might state or imply that the copyright holder endorses your distribution or in any way that might state or imply that you created the original software. Modified distributions must carry, from the original distribution, significantly different names and must not be confused with the original distribution.*
