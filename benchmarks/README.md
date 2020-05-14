# Benchmark-driven development

Making decisions based on scientific benchmarking **while** you develop can guide you to create very efficient solutions if you have the dicipline to follow through. µWebSockets performs with **98%** the theoretical maximum for any user space Linux process - if anything would ever be faster, it would only be so by less than 2%. We know of no such project.

Http | WebSockets
--- | ---
![](../misc/bigshot_lineup.png) | ![](../misc/websocket_lineup.png)

Because of the big lead in cleartext performance, it's actually possible to enable TLS 1.3 encryption in µWebSockets and still beat most of the competition in an unfair cleartext-vs-encrypted run. Performance retention of TLS 1.3 encryption with µWebSockets is about 60%, so you do the math.

All of this is possible thanks to extensive benchmarking of many discarded prototypes & designs during development. The very first thing done in this project was to benchmark the Linux kernel against itself, to get a clear idea of expected maximum performance and thus a performance budget on this platform.

From that point every line of code was benchmarked against the budget and thrown away if it failed the vision. Today µWebSockets does WebSocket messaging without any significant overhead, making it very unlikely to ever be outperformed.

Of course, memory usage has always been a big factor in this. The name µWebSockets is meant to signify "small WebSockets" and comes from the memory optimizations made throughout. Holding many WebSockets should not require lots of RAM.

If you're looking for a performant solution, look no further.
