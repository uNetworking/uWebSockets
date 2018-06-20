# v0.15 of µWebSockets

v0.15 is a complete rewrite of µWS using µSockets as foundation. This leads to a much improved code base with real separation between this and that. It makes it possible to write plugins that only touches what they aim to change. Large performance improvements are inbound for small sends via better corking, and larger sends via proper streams support. SSL is supposed to be a first class citizen as well.

## Focus on HTTP
* Stage 1: Focus on nailing fundamental HTTP(S).
* Stage 2: Better it with built-in router via `Hub::onHttpRoute` helper.
* Stage 3: Think about chunked responses (EventSource).

#### Streams and helpers
HttpSockets are proper streams in v0.15 and do scale for large sends as well as for small ones. "Streams" are in/out per-socket-callbacks for giving or storing streamed data.
```c++
// Seamless swapping between SSL / non-SSL by using uWS::Context() instead!
uWS::SSLContext(options).onHttpRequest([buffer](auto *s, HttpRequest *req) {

    if (req->getUrl() == "/") {
        s->writeStatus(200)->writeHeader("Hello", "World")->end(buffer, 512);
    } else {
        std::cout << "Got HTTP request at URL: " << req->getUrl() << std::endl;
    }

}).listen("localhost", 3000, 0);
```

#### HTTP router helpers
Fast and simple routing should be added under Hub::onHttpRoute(lalala
