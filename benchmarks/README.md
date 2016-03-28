These are the benchmarks used in testing the different WebSocket server implementations. I use WebSocket++ as clients, since that is the fastest client library that I know of, and since you can run multiple instances of the client, it shouldn't really matter what client you use as long as the server is being stressed fully.

* bench1 is a connection and memory scaling benchmark. It creates, as an example, 500k connections and measures the memory usage of the server, and times the connection performance.
* bench 2 is the short message throughput benchmark and connects, as an example, 10k connections and sends a short fix string as binary payload from randomly selected sockets all the time, while timing the delay.
* bench 3 is the huge message throughput benchmark and connects 1 client that sends, as an example, 100mb large binary payload messages while timing the delay.

Echo servers used in testing are presented in this folder as: uWS.cpp, wsPP.cpp, ws.js and lws.cpp.

Make sure to stress the server fully. This means you need to split the work up into multiple client instances if the server is not at 100% CPU. As an example, I run the bench3 split over 3 client instances. I also run bench2 split over 1-4 instances with split load. Bench1 can be run as one instance as that benchmark is enough to stress any server.