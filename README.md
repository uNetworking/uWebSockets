<div align="center"><img src="logo.png"/></div>
`µWS` is one of the most lightweight, efficient & scalable WebSocket server implementations available. It features an easy-to-use, fully async object-oriented interface and scales to millions of connections using only a fraction of memory compared to the competition. While performance and scalability are two of our top priorities, we consider security, stability and standards compliance paramount. License is zlib/libpng (very permissive & suits commercial applications).

* Autobahn tests [all pass](http://htmlpreview.github.io/?https://github.com/alexhultman/uWebSockets/blob/master/autobahn/index.html).
* Linux, OS X & Windows support.
* Valgrind clean.
* Built-in load balancing and multi-core scalability.
* SSL/TLS support & integrates with foreign HTTPS servers.
* Permessage-deflate built-in.
* Node.js binding exposed as the well-known `ws` interface.
* 10-300x faster than `ws` (if they are "fastest", we are "fastester").
* Default engine in SocketCluster & deepstream.io, optional in Socket.IO & Primus.

[![npm version](https://badge.fury.io/js/uws.svg)](https://badge.fury.io/js/uws) [![](https://api.travis-ci.org/alexhultman/uWebSockets.svg?branch=master)](https://travis-ci.org/alexhultman/uWebSockets) [![](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/alexhultman/uWebSockets)

## Benchmarks table - [validate](https://github.com/alexhultman/uWebSockets/tree/master/benchmarks#websocket-echo-server-benchmarks)
Implementation | User space memory scaling | Connection performance | Short message throughput | Huge message throughput
--- | --- | --- | --- | ---
libwebsockets 2.0 | µWS is **11x** as lightweight | µWS is **equal** in performance | µWS is **6x** as performant | µWS is **4x** in performance
ws v1.1.0 + binary addons | µWS is **47x** as lightweight | µWS is **18x** as performant | µWS is **33x** as performant | µWS is **2x** as performant
WebSocket++ v0.7.0 | µWS is **63x** as lightweight | µWS is **4x** as performant | µWS is **3x** as performant | µWS is **2x** as performant
Kaazing Gateway Community 5.0.0 | µWS is **62x** as lightweight | µWS is **15x** as performant | µWS is **18x** as performant | unable to measure

*Benchmarks are run with default settings in all libraries, except for `ws` which is run with the native performance addons. These results were achieved with the native C++ server, not the Node.js addon. Expect worse performance and scalability when using Node.js (don't worry, the Node.js addon will run circles around `ws`).*

## Built with µWS
<table>
<tr>
<td align="center"><a href="https://deepstream.io/"><img src="https://avatars3.githubusercontent.com/u/9024218?v=3&s=200" height="64" /></a></td>
<td align="center"><a href="http://socketcluster.io/#!/"><img src="https://camo.githubusercontent.com/1e6a52dbf401b60f5979aec6416967a42aab8e53/68747470733a2f2f7261772e6769746875622e636f6d2f536f636b6574436c75737465722f736f636b6574636c75737465722f6d61737465722f6173736574732f6c6f676f2e706e67" height="64" /></a></td>
<td align="center"><a href="http://wilds.io/"><img src="https://scontent-ams3-1.xx.fbcdn.net/v/t1.0-1/c43.0.160.160/p160x160/13237648_988335957953290_8996720199169630743_n.png?oh=3a190760956a38db5216efdee1b42646&oe=58363BF4" height="64" /></a></td>
<td align="center"><a href="http://crisp.im/"><img src="https://avatars0.githubusercontent.com/u/16270189?v=3&s=200" height="64" /></a></td>
<td align="center"><a href="https://github.com/silverwind/droppy"><img src="https://camo.githubusercontent.com/abce96075e1d9ffb897a243c09f320d99d0309d7/68747470733a2f2f63646e2e7261776769742e636f6d2f73696c76657277696e642f64726f7070792f6d61737465722f636c69656e742f696d616765732f726561646d652d6c6f676f2e737667" height="64" /></a></td>
</tr>
<tr>
<td align="center">deepstream.io</td>
<td align="center">SocketCluster</td>
<td align="center">wilds.io</td>
<td align="center">Crisp.im</td>
<td align="center">droppy</td>
</tr>
</table>

## Usage

### Node.js
We built `µWS` with the existing Node.js infrastructure in mind. That's why we target the widespread `ws` interface, allowing us to seamlessly integrate with projects like SocketCluster, deepstream.io, Socket.IO & Primus.

* Read the [ws documentation](https://github.com/websockets/ws/blob/master/doc/ws.md)
* Read the [Primus transformer documentation](https://github.com/primus/primus#uws)

There are some important incompatibilities with `ws` though, we aim to be ~90% compatible but will never implement behavior that is deemed too inefficient:

* Binary data is passed zero-copy as an `ArrayBuffer`. This means you need to copy it to keep it past the callback. It also means you need to convert it with `Buffer.from(message)` if you expect a `Node.js Buffer`.
* `webSocket._socket` is not a `net.Socket`, it is just a getter function with very basic functionalities.
* `webSocket._socket.remote...` might fail, you need to cache it at connection.
* `webSocket` acts like an `EventEmitter` with one listener per event maximum.
* `webSocket.upgradeReq` is very limited and only holds commonly accessed data.

##### SocketCluster
`µWS` is the default engine in [SocketCluster](http://socketcluster.io) as of 5.0.0.

##### deepstream.io
`µWS` is the default engine in [deepstream.io](http://deepstream.io/) as of 1.0.0.

##### Socket.IO
Use the new `wsEngine: 'uws'` option like so:
```javascript
var io = require('socket.io')(80, { wsEngine: 'uws' });
```
This option has not yet been released, one alternative way of enabling `uws` in current versions of Socket.IO is:
```javascript
var io = require('socket.io')(80);
io.engine.ws = new (require('uws').Server)({
    noServer: true,
    perMessageDeflate: false
});
```
##### Primus
Set 'uws' as transformer:
```javascript
var primus = new Primus(server, { transformer: 'uws' });
```
##### ws
If your code directly relies on `ws` you can simply swap `require('ws')` with `require('uws')`:
```javascript
var WebSocketServer = require('uws').Server;
var wss = new WebSocketServer({ port: 8080 });

wss.on('connection', function (ws) {
    ws.on('message', function (message) {
        console.log('received: ' + message);
    });

    ws.send('something');
});
```
### C++
For maximum performance and memory scaling the native interface is recommended. Look in the examples folder for threading and load balancing examples. There is no documentation written yet but a bright person like you will have no problem just reading the header file.
```c++
int main()
{
    /* this is an echo server that properly passes every supported Autobahn test */
    uWS::Server server(3000);
    server.onConnection([](uWS::WebSocket socket) {
        cout << "[Connection] clients: " << ++connections << endl;
    });

    server.onMessage([](uWS::WebSocket socket, char *message, size_t length, uWS::OpCode opCode) {
        socket.send(message, length, opCode);
    });

    server.onDisconnection([](uWS::WebSocket socket) {
        cout << "[Disconnection] clients: " << --connections << endl;
    });

    server.run();
}
```

## Quality control
* Valgrind clean.
* Autobahn tests [all pass](http://htmlpreview.github.io/?https://github.com/alexhultman/uWebSockets/blob/master/autobahn/index.html).
* All Primus transformer integration tests pass.
* All Engine.IO server tests pass.
* Small & efficient code base.

## Installation
### Node.js developers
[![](https://nodei.co/npm/uws.png)](https://www.npmjs.com/package/uws)

* Node.js 4.x, 5.x & 6.x supported
* Linux, Mac OS X & Windows supported

*Windows version requires Node.js 6.4.0+*

### C++ developers
#### Dependencies
First of all you need to install the required dependencies. On Unix systems this is typically done via package managers, like [homebrew](http://brew.sh) in the case of OS X or `dnf` in the case of Fedora Linux. On Windows you need to search the web for pre-compiled binaries or simply compile the dependencies yourself.

* libuv 1.x
* OpenSSL 1.0.x
* zlib 1.x
* CMake 3.x

#### Compilation
Obviously you will need to clone this repo to get the sources. We use CMake as build system.

* `git clone https://github.com/alexhultman/uWebSockets.git && cd uWebSockets`
* `cmake .`

Now, on Unix systems it should work by simply running `make`. Run [sudo] `make install` as you wish.

##### Windows, in all its glory
If you are running Windows you should now have a bunch of Visual Studio project files and one solution file. Open the solution file, now you need to make sure the header include paths and library paths are all set according to where you installed the dependencies. You might also need to change the names of the libraries being linked against, all according to the names of the installed library files. You know the drill.
