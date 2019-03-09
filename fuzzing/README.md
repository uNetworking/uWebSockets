# Fuzz-testing of parsers

Here we do coverage-based fuzzing of code responsible for parsing arbitrary network data.

A secure web server must be capable of receiving mass amount of malicious input without showing signs of weakness. Test code is being bombarded with evolving random data, with fitness determined by coverage - the goal of seeking out as much of the program state space as possible. This is done while AddressSanitizer monitors the program for memory correctness.