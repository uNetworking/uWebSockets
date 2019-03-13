# libFuzzer/AddressSanitizer fuzz-testing of various parsers

Here we do coverage-based fuzzing of code responsible for parsing arbitrary network data.

A secure web server must be capable of receiving mass amount of malicious input without misbehaving or performing illegal actions. Test code is being bombarded with evolving random data, with fitness determined by coverage - that is we aim to seek out as much of the "branching space" of the program as possible. This is done while AddressSanitizer monitors the program for memory correctness.

Currently the following parts are individually tested:

* WebSocket handshake generator
* WebSocket message parser
* WebSocket extensions parser & negotiator
* Http parser
* Http method/url router

No defects or issues are left unfixed.
