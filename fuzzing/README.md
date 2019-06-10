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

### While entire (mocked) examples are fuzzed:

* HelloWorld

No defects or issues are left unfixed, covered up or otherwise neglected.
