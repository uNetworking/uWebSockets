# WebSocket echo server benchmarks
By reading this you have proven yourself to be above 95% of all programmers. Most programmers nowadays give absolutely zero shit to actually *validate* wild and crazy performance claims they stumble upon. You get performance claims shoved down your throat from all over the internet nowadays, but in reality, performance of most software has only degraded over the last 20 years. When I began writing µWebSockets I was blown away by my findings from actually benchmarking these wild claims. Surely I had messed up somehow!

As opposed to many of the top performance claimers like `Socket.IO` ("FEATURING THE FASTEST AND MOST RELIABLE REAL-TIME ENGINE") and `ws` ("ws: The fastest RFC-6455 WebSocket implementation for Node.js."), I actually aim to be transparent in my benchmarkings, and give you the opportunity to easily validate my claims. Instead of just making up random ass bullshit like these projects do, I actually spent quite the time optimizing µWebSockets.

Hawk eye achievement: You noticed the use of *superlatives* in these descriptions. Quoting Wikipedia: "In grammar, the superlative is the form of an adverb or adjective that is used to signify the greatest degree of a given descriptor." *facepalm*

### A note about bottlenecks
Problem number one when benchmarking a server is to actually benchmark *the server*. Quite a few programmers do not realize that if the client that is supposed to benchmark the server is the bottleneck, you are really just benchmarking the client. This is why I supply two highly optimized and specialized benchmarking clients that successfully can stress µWebSockets to 100% CPU usage – something not possible with any of the clients I have tested.

## Building instructions
* Benchmarks depend on Linux & libuv, but you could port them to OS X and Windows if you want
* WebSocket++ and Boost is needed to build the WebSocket++ echo server (wsPP) - *rly, no shit?!*
* Node is needed to run the ws.js echo server

Once you have installed `libuv` all you need is to hit `make` inside of uWebSockets/benchmarks (this folder) and you'll get the binaries used in these benchmarks.

## Scalability
The first and in my opinion most valuable benchmark is about scalability. The name µWebSockets is a hint of its small ("micro") WebSockets in terms of memory footprint.

`Usage: scalability numberOfConnections port`

I run this test with 1 million connections on port 3000 when testing µWS, and 500k connections when testing the two other servers (simply because I have no RAM to reach 1 million connections with these servers).

This benchmark will measure the memory usage of the process that listens to the given port, so it can automatically report results like this:
```
Connection performance: 72.4323 connections/ms
Memory performance: 2695.25 connections/mb
```

## Throughput
The second benchmark is a little more complex as it takes 4 arguments:

`Usage: throughput numberOfConnections payloadByteSize framesPerSend port`

#### Short message throughput
These are the arguments I supply to benchmark the "short message throughput":

`./throughput 10000 20 10 3000`

This will connect 10k users and start to iterate while taking the time between iteration. Every iteration will send 10k messages spread over randomly chosen sockets. The payload is 20 byte long and every send is a pack of 10 frames.

Output is in the form of "echoes per millisecond" and is averaged over the entire time the benchmark runs. If you send a pack of 10 messages per send, these will count as 10 messages.

##### Plausible controversy
There is this thing called confirmation bias - when one wants to badly "prove" their stance they can easily get carried away by making up invalid proofs. Since I wrote the server, and I also wrote the benchmark and supplied the arguments to it, there is a plausible controversy that I will discuss.

There is one way to achieve a far smaller difference between the servers, and that is by supplying `1` as the argument for framesPerSend. This will not stress the WebSocket protocol parser as much, but rather only stress the epoll/event system. That's why I want to supply 10 to really stress the server maximally. If you supply `1` here, the difference between `ws` and `µWS` will drop from about 33x to 6x, and the difference between `WebSocket++` and `µWS` will drop from 3x to 2x. Personally, I think it is fair to supply 10 - otherwise you are not really benchmarking anything else than the event system.

#### Huge message throughput
There is no controversy involved in the huge message throughput benchmark, just run this and it will send 100mb of payload as fast as the server manages to echo it back:

`./throughput 1 104857600 1 3000`

*Happy benchmarkings!*