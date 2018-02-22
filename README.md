<div align="center"><img src="misc/images/logo.png"/></div>

µWS ("[micro](https://en.wikipedia.org/wiki/Micro-)WS") is a WebSocket and HTTP implementation for clients and servers. Simple, efficient and lightweight.

[Read more](DETAILS.md)

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

#### Excel across the board.
<div align="center"><img src="misc/images/overview.png"/></div>

#### Freely available.
Non-profit open source ([Zlib](LICENSE)) since 2016.
