<div align="center">
<img src="misc/logo.png" />

*µWS ("[micro](https://en.wikipedia.org/wiki/Micro-)WS") is simple and efficient*<sup>[[1]](benchmarks)</sup> *messaging for the modern web.*

• [Fancy pants details, user manual and FAQ](misc/READMORE.md)

</div>

#### Express yourself briefly.
```c++
uWS::SSLApp({

    /* There are tons of SSL options */
    .cert_file_name = "cert.pem",
    .key_file_name = "key.pem"
    
}).onGet("/", [](auto *res, auto *req) {

    /* Respond with the web app on default route */
    res->writeStatus("200 OK")
       ->writeHeader("Content-Type", "text/html; charset=utf-8")
       ->end(indexHtmlBuffer);
    
}).onWebSocket<UserData>("/ws/chat", [&](auto *ws, auto *req) {

    /* Subscribe to topic /chat */
    ws->subscribe("chat");
    
}).onMessage([&](auto *ws, auto message, auto opCode) {

    /* Parse incoming message according to some protocol & publish it */
    if (seemsReasonable(message)) {
        ws->publish("chat", message);
    } else {
        ws->close();
    }
    
}).onClose([&](auto *ws, int code, auto message) {

    /* Remove websocket from this topic */
    ws->unsubscribe("chat");
    
}).listen("localhost", 3000, 0).run();
```

#### Pay what you want.
A free & open source ([Zlib](LICENSE)) project since 2016. Kindly sponsored by [BitMEX](https://bitmex.com), [Bitfinex](https://bitfinex.com) & [Coinbase](https://www.coinbase.com/) in 2018.

<div align="center"><img src="misc/2018.png"/></div>

*Become a paying sponsor to unlock support, issue reporting, roadmaps and to drop suggestions.*

#### Deploy like a boss.
Commercial support is available via a per-hourly consulting plan or as otherwise negotiated. If you're stuck, worried about design or just in need of help don't hesitate throwing [me, the author](https://github.com/alexhultman) a mail and we'll figure out what's best for both parties. I want your business to have a proper understanding of the problem before rushing in to one of the many pitfalls.

#### Excel across the board.
![](misc/bigshot_lineup.png)
