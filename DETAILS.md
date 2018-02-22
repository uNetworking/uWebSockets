#### Features

* Autobahn tests [all pass](http://htmlpreview.github.io/?https://github.com/uWebSockets/uWebSockets/blob/master/misc/autobahn/index.html).
* One million WebSockets require ~111mb of user space memory (104 bytes per WebSocket).
* Single-threaded throughput of up to 5 million HTTP req/sec or 20 million WebSocket echoes/sec.
* Linux, OS X, Windows & [Node.js](http://github.com/uWebSockets/bindings) support.
* Runs with raw epoll, libuv or ASIO (C++17-ready).
* Valgrind & AddressSanitizer clean.
* Permessage-deflate, SSL/TLS support & integrates with foreign HTTP(S) servers.
* Multi-core friendly & optionally thread-safe via compiler flag UWS_THREADSAFE.

#### Dependencies
First of all you need to install the required dependencies. This is very easily done with a good open source package manager like [Homebrew](http://brew.sh) for OS X, [vcpkg](https://github.com/Microsoft/vcpkg) for Windows or your native Linux package manager.

* OpenSSL 1.x.x
* zlib 1.x
* libuv 1.3+ *or* Boost.Asio 1.x (both optional on Linux)

If you wish to integrate with a specific event-loop you can define `USE_ASIO` or `USE_LIBUV` as a global compilation flag and then link to respective libraries. `USE_EPOLL` is default on Linux while other systems default to `USE_LIBUV`.

* Fedora: `sudo dnf install openssl-devel zlib-devel`
* Homebrew: `brew install openssl zlib libuv`
* Vcpkg: `vcpkg install openssl zlib libuv` *and/or* `vcpkg install openssl:x64-windows zlib:x64-windows libuv:x64-windows`

#### Compilation
###### OS X & Linux
* `make`
* `sudo make install` (or as you wish)
###### Windows
* Compile `VC++.vcxproj` with Visual C++ Community Edition 2015 or later.
