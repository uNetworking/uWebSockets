<div align="center">
<img src="https://raw.githubusercontent.com/uNetworking/uWebSockets/master/misc/logo.svg" height="180" /><br>
<i>简单，安全</i><sup><a href="https://github.com/uNetworking/uWebSockets/tree/master/fuzzing#fuzz-testing-of-various-parsers-and-mocked-examples">1</a></sup><i> & 标准化</i><sup><a href="https://unetworking.github.io/uWebSockets.js/report.pdf">2</a></sup><i>的网络服务器，适用于对性能要求最苛刻的</i><sup><a href="https://github.com/uNetworking/uWebSockets/tree/master/benchmarks#benchmark-driven-development">3</a></sup><i>应用程序。</i> <a href="https://github.com/uNetworking/uWebSockets/blob/master/misc/READMORE.md">了解更多...</a>
<br><br>

<a href="https://github.com/uNetworking/uWebSockets/releases"><img src="https://img.shields.io/github/v/release/uNetworking/uWebSockets"></a> <a href="https://osv.dev/list?q=uwebsockets&affected_only=true&page=1&ecosystem=OSS-Fuzz"><img src="https://oss-fuzz-build-logs.storage.googleapis.com/badges/uwebsockets.svg" /></a> <img src="https://img.shields.io/badge/est.-2016-green" /> <a href="https://twitter.com/uNetworkingAB"><img src="https://raw.githubusercontent.com/uNetworking/uWebSockets/master/misc/follow.png" height="20"/></a>

</div>
<br><br>

### :closed_lock_with_key: 优化的安全性能
µWebSockets经过精心优化，在运行速度和内存占用方面的表现都非常出色，其处理TLS 1.3加密消息的速度比其它大多数服务器处理未加密的明文消息还要快<sup><a href="https://github.com/uNetworking/uWebSockets/tree/master/benchmarks#benchmark-driven-development">3</a></sup>。

此外，我们参与了Google的OSS-Fuzz项目，日常的模糊测试覆盖率约为95%<sup><a href="https://github.com/uNetworking/uWebSockets/blob/master/misc/Screenshot_20210915-004009.png?raw=true">4</a></sup>，且没有发现任何sanitizer问题。LGTM因为我们零CodeQL告警给我们打出了完美的A+分数，我们还是使用严格的警告级别进行编译。

### :arrow_forward: 快速脚本编写
µWebSockets完全用C和C++编写，但与 Node.js 无缝集成。这允许快速编写强大的应用程序。参见<a href="https://github.com/uNetworking/uWebSockets.js">µWebSockets.js</a>。

### :crossed_swords: 经过实战考验
自2016年以来，我们完全符合标准，Autobahn|Testsuite的得分是完美的<sup><a href="https://unetworking.github.io/uWebSockets.js/report.pdf">2</a></sup>。µWebSockets为世界上许多最大的加密货币交易所提供支持，每天处理数十亿美元的交易量。如果你交易加密货币，你很可能是通过µWebSockets进行的。

### :battery: 包含所有必需组件
围绕一个简便的URL路由器设计，支持通配符和参数————与WebSockets的高效pub/sub特性搭配。µWebSockets应该是任何实时网络项目首选和最终的方案，这些项目有很高的要求。

立即开始构建你的Http和WebSocket应用程序；<a href="https://github.com/uNetworking/uWebSockets/blob/master/misc/READMORE.md">阅读用户手册</a>和<a href="https://github.com/uNetworking/uWebSockets/tree/master/examples">查看示例</a>。你可以浏览我们的<a href="https://unetworking.github.io/uWebSockets.js/generated/">TypeDoc</a>快速概览。

```c++
/* 每个线程一个应用；根据CPU核心数生成，让uWS共享监听端口 */
uWS::SSLApp({

    /* 这些是最常见的选项，全链证书和密钥。更多选项请参见uSockets。 */
    .cert_file_name = "cert.pem",
    .key_file_name = "key.pem"
    
}).get("/hello/:name", [](auto *res, auto *req) {

    /* 你也可以高效地流式传输大文件 */
    res->writeStatus("200 OK")
       ->writeHeader("Content-Type", "text/html; charset=utf-8")
       ->write("<h1>Hello ")
       ->write(req->getParameter("name"))
       ->end("!</h1>");
    
}).ws<UserData>("/*", {

    /* 这只是一些可用的处理程序 */
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
### :briefcase: 商业支持
<a href="https://github.com/uNetworking">uNetworking AB</a>是一家瑞典咨询和承包公司，处理与µWebSockets相关的一切事务；开发、支持和客户成功🏅️。

如果你正在构建大型项目，需要建议或有其他商业咨询，请毫不犹豫地<a href="mailto:alexhultman@gmail.com">发送邮件</a>。我们将为双方找出最佳解决方案，并确保你不会轻易地踩坑。

特别感谢BitMEX、Bitfinex、Google、Coinbase、Bitwyre、AppDrag和deepstreamHub，它们允许该项目自2016年以来在GitHub上蓬勃发展——没有这些优秀的公司，这个项目是不可能的。

### :wrench: 可定制的架构
µWebSockets基于<a href="https://github.com/uNetworking/uSockets">µSockets</a>，这是一个基础库，封装了事件、网络和加密三个层次的实现。每个层次都有多种实现方式，这可以通过标志控制编译组合。目前有五种事件循环集成；libuv、ASIO、GCD和原生epoll/kqueue。

简而言之：

* `WITH_WOLFSSL=1 WITH_LIBUV=1 make examples` 构建使用WolfSSL和libuv的示例
* `WITH_OPENSSL=1 make examples` 构建使用OpenSSL和原生内核的示例

有关标志的最新列表和更详细的解释，请参见µSockets。

### :handshake: 开放许可
知识产权，保留所有权利。

在给出此类明确通知的地方，源代码根据Apache License 2.0许可，这是一种开放的OSI批准的许可，几乎没有限制。修改后的“分支”应该只有许可源代码，并在另一个产品名称下提供。如果你对这些有任何疑问，请放心大胆地询问。
