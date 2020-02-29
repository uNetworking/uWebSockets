# Fuzz-testing of various parsers and mocked examples

A secure web server must be capable of receiving mass amount of malicious input without misbehaving or performing illegal actions, such as stepping outside of a memory block or otherwise spilling the beans.

### Continuous fuzzing under various sanitizers is done as part of the [Google OSS-Fuzz](https://github.com/google/oss-fuzz#oss-fuzz---continuous-fuzzing-for-open-source-software) project:
* UndefinedBehaviorSanitizer
* AddressSanitizer
* MemorySanitizer

### Currently the following parts are individually fuzzed:

* WebSocket handshake generator
* WebSocket message parser
* WebSocket extensions parser & negotiator
* WebSocket permessage-deflate compression/inflation helper
* Http parser
* Http method/url router
* Pub/sub "topic tree"

### While entire (mocked) examples are fuzzed:

* HelloWorld
* EchoServer
* BroadcastingEchoServer

No defects or issues are left unfixed, covered up or otherwise neglected. In fact we **cannot** cover up security issues as OSS-Fuzz automatically and publicly reports security issues as they happen.

Here is the list of public issues (issues are kept private for 90 days or until fixed): https://bugs.chromium.org/p/oss-fuzz/issues/list?q=label%3AProj-uwebsockets&can=1

Currently we are at ~90% total fuzz coverage and OSS-Fuzz is reporting **zero** issues whatsoever. The goal is to approach 100% total coverage.

### Security awards
Google have sent us thousands of USD for the integration with OSS-Fuzz - we continue working on bettering the testing with every new release.
