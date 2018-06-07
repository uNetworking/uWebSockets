<div align="center"><img src="misc/images/logo.png"/></div>

µWS ("[micro](https://en.wikipedia.org/wiki/Micro-)WS") is a WebSocket and HTTP implementation for clients and servers. Simple, efficient and lightweight.

[Wiki pages & user manual](https://github.com/uNetworking/uWebSockets/wiki/User-manual-v0.14.x) | [Care for a sneak peek?](https://github.com/uNetworking/uSockets)

#### Build optimized WebSocket & HTTP servers & clients in no time.
```c++
#include <uWS/uWS.h>
using namespace uWS;

int main() {
    Hub h;
    std::string response = "Hello!";

    h.onMessage([](WebSocket<SERVER> *ws, char *message, size_t length, OpCode opCode) {
        ws->send(message, length, opCode);
    });

    h.onHttpRequest([&](HttpResponse *res, HttpRequest req, char *data, size_t length,
                        size_t remainingBytes) {
        res->end(response.data(), response.length());
    });

    if (h.listen(3000)) {
        h.run();
    }
}
```

#### Pay what you want.
A free & open source ([Zlib](LICENSE)) hobby project of [mine](https://github.com/alexhultman) since 2016. Kindly sponsored by [BitMEX](https://bitmex.com), [Bitfinex](https://bitfinex.com) & [Coinbase](https://www.coinbase.com/) in 2018.

<div align="center"><img src="misc/images/2018.png"/></div>

*Understand I don't take issue reports, suggestions or provide any support to free-riders. You want in? Become a sponsor.*

#### Excel across the board.
<div align="center"><img src="misc/images/overview.png"/></div>

#### Be fast, not broken.
Gracefully passes the [entire Autobahn fuzzing test suite](http://htmlpreview.github.io/?https://github.com/uNetworking/uWebSockets/blob/master/misc/autobahn/index.html) with no failures or Valgrind/ASAN errors. With or without SSL/permessage-deflate.
