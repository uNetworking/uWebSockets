# libFuzzer/AddressSanitizer fuzz-testing of various parsers

Here we do coverage-based fuzzing of code responsible for parsing arbitrary network data.

A secure web server must be capable of receiving mass amount of malicious input without misbehaving or performing illegal actions. Test code is being bombarded with evolving random data, with fitness determined by coverage - that is we aim to seek out as much of the "branching space" of the program as possible. This is done while AddressSanitizer monitors the program for memory correctness.

Currently the following parts are individually tested:

* WebSocket handshake generator
* WebSocket message parser
* WebSocket extensions parser & negotiator
* WebSocket permessage-deflate compression/inflation helper
* Http parser
* Http method/url router

No defects or issues are left unfixed.

## Mocking
If you think about it, uWebSockets is really just a piece of data transformation code. Being standard C++ with no dependencies other than uSockets, all it does is:

* read & parse data
* act on & handle data
* format & write back data

If we mock the uSockets interface and link to it instead of the real thing, we can actually fuzz-test the entire project in a more integration-testy way rather than the above mentioned unit-testy way.

* A mock server is created
* libFuzzer emits data which we hack up in pieces and emit to uWebSockets via uSockets.
* Whatever happens next, happens.
* Fix bugs.
