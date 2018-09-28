## FAQ
You can read the frequently asked questions [here](FAQ.md).

## Motivation and goals
µWebSockets is a simple to use yet thoroughly optimized implementation of HTTP and WebSockets.
It comes with built-in pub/sub support, HTTP routing, TLS 1.3, IPv6, permessage-deflate and is thorougly battle tested as one of the most popular implementations.
Unlike other "pub/sub brokers", µWS does not assume or push any particular protocol but only operates over standard WebSockets.

The implementation is header-only C++17, cross-platform and compiles down to a tiny binary of a handful kilobytes.
It depends on µSockets, which is a standard C project for Linux, macOS & Windows.

Performance wise you can expect to outperform just about anything out there, that's the goal of the project.
I can show cases where µWS with SSL significantly outperforms Golang servers running non-SSL. You get the SSL for free in a sense.

Another goal of the project is minimalism, simplicity and elegance.
Design wise it follows an ExpressJS-like interface where you attach callbacks to different URL routes.
This way you can easily build complete REST/WebSocket services in a few lines of code.

The project is async only and runs local to one thread. You scale it as individual threads much like Node.js scales as individual processes. That is, the implementation only sees a single thread and is not thread-safe. There are simple ways to do threading via async delegates though, if you really need to.

## Node.js
µWS is available as "uws" for Node.js where it can serve as a major boost for web related tasks. You can find "uws" on this page.

## User manual
todo

