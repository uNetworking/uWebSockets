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

Currently we are at ~80% total fuzz coverage and OSS-Fuzz is reporting **zero** issues whatsoever. The goal is to approach 90% total coverage.

### Security awards
Google have sent us thousands of USD for the integration with OSS-Fuzz - we continue working on bettering the testing with every new release.
