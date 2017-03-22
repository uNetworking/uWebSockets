<div align="center"><img src="misc/images/logo.png"/></div>
`µWS` is one of the most lightweight, efficient & scalable WebSocket & HTTP server implementations available. It features an easy-to-use, fully async object-oriented interface and scales to millions of connections using only a fraction of memory compared to the competition. While performance and scalability are two of our top priorities, we consider security, stability and standards compliance paramount. License is zlib/libpng (very permissive & suits commercial applications).

* Autobahn tests [all pass](http://htmlpreview.github.io/?https://github.com/uWebSockets/uWebSockets/blob/master/misc/autobahn/index.html).
* One million WebSockets require ~122mb of user space memory (112 bytes per WebSocket).
* By far one of the fastest in both HTTP and WebSocket throughput (see table below).
* Linux, OS X, Windows & [Node.js](nodejs) support.
* Runs with raw epoll, libuv or ASIO (C++17-ready).
* Valgrind & AddressSanitizer clean.
* Permessage-deflate, SSL/TLS support & integrates with foreign HTTP(S) servers.
* Multi-core friendly.

[![](https://api.travis-ci.org/uWebSockets/uWebSockets.svg?branch=master)](https://travis-ci.org/uWebSockets/uWebSockets) [![](misc/images/patreon.png)](https://www.patreon.com/uWebSockets)

## Simple & modern
The C++ interface has been designed for simplicity and only requires you to write a few lines of code to get a working server:
```c++
#include <uWS/uWS.h>

int main() {
    uWS::Hub h;

    h.onMessage([](uWS::WebSocket<uWS::SERVER> ws, char *message, size_t length, uWS::OpCode opCode) {
        ws.send(message, length, opCode);
    });

    h.onHttpRequest([](uWS::HttpResponse *res, uWS::HttpRequest req, char *data, size_t length, size_t remainingBytes) {
        res->end(const char *, size_t);
    });

    h.listen(3000);
    h.run();
}
```
Get the sources of the uws.chat server [here](https://github.com/uWebSockets/website/blob/master/main.cpp). Learn from the tests [here](tests/main.cpp).

## Widely adopted
<div align="center"><img src="misc/images/builtwithuws.png"/></div>

## Not your average server
Implementation | User space memory scaling | Connection performance | Short message throughput | Huge message throughput
--- | --- | --- | --- | ---
Beast 1.0.0 b17 | µWS is **7x** as lightweight | µWS is **4x** as performant | µWS is **22x** as performant | µWS is **3x** as performant
libwebsockets 2.0 | µWS is **11x** as lightweight | µWS is **equal** in performance | µWS is **6x** as performant | µWS is **4x** as performant
Crow [Sep 21] | µWS is **13x** as lightweight | µWS is **2x** as performant | µWS is **12x** as performant | data missing
Gorilla e8f0f8a | µWS is **46x** as lightweight | µWS is **3x** as performant | µWS is **5x** as performant | data missing
ws v1.1.0 + binary addons | µWS is **47x** as lightweight | µWS is **18x** as performant | µWS is **33x** as performant | µWS is **2x** as performant
Kaazing Gateway Community 5.0.0 | µWS is **62x** as lightweight | µWS is **15x** as performant | µWS is **18x** as performant | data missing
Socket.IO 1.5.1 | µWS is **62x** as lightweight | µWS is **42x** as performant | µWS is **61x** as performant | data missing
WebSocket++ v0.7.0 | µWS is **63x** as lightweight | µWS is **4x** as performant | µWS is **3x** as performant | µWS is **2x** as performant

*Benchmarks are run with default settings in all libraries, except for `ws` which is run with the native performance addons. Read more about the benchmarks [here](benchmarks).*

## Getting started
#### Dependencies
First of all you need to install the required dependencies. This is very easily done with a good open source package manager like [Homebrew](http://brew.sh) for OS X, [vcpkg](https://github.com/Microsoft/vcpkg) for Windows or your native Linux package manager.

Always required:
* OpenSSL 1.0.x
* zlib 1.x

Not required on Linux systems:
* libuv 1.3+
* Boost.Asio 1.x

On Linux systems you don't necessarily need any third party event-loop library, but can run directly on the high performance epoll backend (this gives by far the best performance and memory usage). Non-Linux systems will automatically fall back to libuv.

If you wish to integrate with a specific event-loop you can define `USE_ASIO` or `USE_LIBUV` as a global compilation flag and then link to respective libraries.

#### Compilation
Clone and enter the repo:
* `git clone https://github.com/uWebSockets/uWebSockets.git && cd uWebSockets`

###### OS X & Linux
Compile with Make:
* `make`
* `sudo make install`

###### Windows
Compile with Visual C++ Community Edition 2015 or later. This workflow requires previous usage of vcpkg:
* Open the VC++ project file
* Click Build
