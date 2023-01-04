
<div align="center">
<img src="https://raw.githubusercontent.com/uNetworking/uWebSockets/master/misc/logo.svg" height="180" /><br>
<i>Simple, secure</i><sup><a href="https://github.com/uNetworking/uWebSockets/tree/master/fuzzing#fuzz-testing-of-various-parsers-and-mocked-examples">1</a></sup><i> & standards compliant</i><sup><a href="https://unetworking.github.io/uWebSockets.js/report.pdf">2</a></sup><i> web server for the most demanding</i><sup><a href="https://github.com/uNetworking/uWebSockets/tree/master/benchmarks#benchmark-driven-development">3</a></sup><i> of applications.</i> <a href="https://github.com/uNetworking/uWebSockets/blob/master/misc/READMORE.md">Read more...</a>
<br><br>

<a href="https://github.com/uNetworking/uWebSockets/releases"><img src="https://img.shields.io/github/v/release/uNetworking/uWebSockets"></a> <a href="https://osv.dev/list?q=uwebsockets&affected_only=true&page=1&ecosystem=OSS-Fuzz"><img src="https://oss-fuzz-build-logs.storage.googleapis.com/badges/uwebsockets.svg" /></a> <img src="https://img.shields.io/badge/downloads-70%20million-pink" />

</div>
<br><br>

### :closed_lock_with_key: Optimized security
Being meticulously optimized for speed and memory footprint, µWebSockets is fast enough to do encrypted TLS 1.3 messaging quicker than most alternative servers can do even unencrypted, cleartext messaging<sup><a href="https://github.com/uNetworking/uWebSockets/tree/master/benchmarks#benchmark-driven-development">3</a></sup>.

Furthermore, we partake in Google's OSS-Fuzz with a ~95% daily fuzzing coverage<sup><a href="https://github.com/uNetworking/uWebSockets/blob/master/misc/Screenshot_20210915-004009.png?raw=true">4</a></sup> with no sanitizer issues. LGTM scores us flawless A+ from having zero CodeQL alerts and we compile with pedantic warning levels.


### :arrow_forward: Rapid scripting
µWebSockets is written entirely in C & C++ but has a seamless integration for Node.js backends. This allows for rapid scripting of powerful apps, using widespread competence. See <a href="https://github.com/uNetworking/uWebSockets.js">µWebSockets.js</a>.

Besides this Node.js integration, you can also [use Bun](https://bun.sh) where µWebSockets is the built-in web server.

### :crossed_swords: Battle proven
We've been fully standards compliant with a perfect Autobahn|Testsuite score since 2016<sup><a href="https://unetworking.github.io/uWebSockets.js/report.pdf">2</a></sup>. µWebSockets powers many of the biggest crypto exchanges in the world, handling trade volumes of multiple billions of USD every day. If you trade crypto, chances are you do so via µWebSockets.

### :battery: Batteries included
Designed around a convenient URL router with wildcard & parameter support - paired with efficient pub/sub features for WebSockets. µWebSockets should be the obvious, complete starting point for any real-time web project with high demands.

Start building your Http & WebSocket apps in no time; <a href="https://github.com/uNetworking/uWebSockets/blob/master/misc/READMORE.md">read the user manual</a> and <a href="https://github.com/uNetworking/uWebSockets/tree/master/examples">see examples</a>. You can browse our <a href="https://unetworking.github.io/uWebSockets.js/generated/">TypeDoc</a> for a quick overview.

```c++
uWS::SSLApp({

    /* These are the most common options, fullchain and key. See uSockets for more options. */
    .cert_file_name = "cert.pem",
    .key_file_name = "key.pem"
    
}).get("/hello", [](auto *res, auto *req) {

    /* You can efficiently stream huge files too */
    res->writeHeader("Content-Type", "text/html; charset=utf-8")->end("Hello HTTP!");
    
}).ws<UserData>("/*", {

    /* Just a few of the available handlers */
    .open = [](auto *ws) {
        ws->subscribe("oh_interesting_subject");
    },
    .message = [](auto *ws, std::string_view message, uWS::OpCode opCode) {
        ws->send(message, opCode);
    }
    
}).listen(9001, [](auto *listenSocket) {

    if (listenSocket) {
        std::cout << "Listening on port " << 9001 << std::endl;
    } else {
        std::cout << "Failed to load certs or to bind to port" << std::endl;
    }
    
}).run();
```
### :briefcase: Commercially supported
<a href="https://github.com/uNetworking">uNetworking AB</a> is a Swedish consulting & contracting company dealing with anything related to µWebSockets; development, support and customer success.

Don't hesitate <a href="mailto:alexhultman@gmail.com">sending a mail</a> if you're building something large, in need of advice or having other business inquiries in mind. We'll figure out what's best for both parties and make sure you're not falling into common pitfalls.

Special thanks to BitMEX, Bitfinex, Google, Coinbase, Bitwyre, AppDrag and deepstreamHub for allowing the project itself to thrive on GitHub since 2016 - this project would not be possible without these beautiful companies.

### :wrench: Customizable architecture
µWebSockets builds on <a href="https://github.com/uNetworking/uSockets">µSockets</a>, a foundation library implementing eventing, networking and cryptography in three different layers. Every layer has multiple implementations and you control the compiled composition with flags. There are currently five event-loop integrations; libuv, ASIO, GCD and raw epoll/kqueue.

In a nutshell:

* `WITH_WOLFSSL=1 WITH_LIBUV=1 make examples` builds examples utilizing WolfSSL and libuv
* `WITH_OPENSSL=1 make examples` builds examples utilizing OpenSSL and the native kernel

See µSockets for an up-to-date list of flags and a more detailed explanation.

### :handshake: Permissively licensed
Intellectual property, all rights reserved.

Where such explicit notice is given, source code is licensed Apache License 2.0 which is a permissive OSI-approved license with very few limitations. Modified "forks" should be of nothing but licensed source code, and be made available under another product name. If you're uncertain about any of this, please ask before assuming.
