**Psst!** v0.15 dropping in a few weeks with major perf. advancements (think at least 33% boost over v0.14).

<div align="center">
<img src="misc/images/logo.png" />
    
*µWS ("[micro](https://en.wikipedia.org/wiki/Micro-)WS") is simple and efficient messaging for the modern web.*

• [Wiki pages & user manual](https://github.com/uNetworking/uWebSockets/wiki/User-manual-v0.14.x)

</div>

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

#### Deploy with confidence.
Feeling uncertain about your design? In need of professional help? I might have a few consulting hours for you and your business, send [me](https://github.com/alexhultman) a mail and we'll figure out the rest.

#### Excel across the board.
<div align="center"><img src="misc/images/overview.png"/></div>

#### Be fast, not broken.
Gracefully passes the [entire Autobahn fuzzing test suite](http://htmlpreview.github.io/?https://github.com/uNetworking/uWebSockets/blob/master/misc/autobahn/index.html) with no failures or Valgrind/ASAN errors. With or without SSL/permessage-deflate.
