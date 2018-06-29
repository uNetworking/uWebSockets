#ifndef TESTS_H
#define TESTS_H

#include "HttpParser.h"

#include <chrono>

void testHttpParserPerformance() {

    char data[] = "GET /hello.htm HTTP/1.1\r\n"
            "User-Agent: Mozilla/4.0 (compatible; MSIE5.01; Windows NT)\r\n"
            "Host: www.tutorialspoint.com\r\n"
            "Accept-Language: en-us\r\n"
            "Accept-Encoding: gzip, deflate\r\n"
            "Connection: Keep-Alive\r\n\r\n ";

    HttpParser httpParser;

    int validRequests = 0;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000000; i++) {
        httpParser.consumePostPadded(data, sizeof(data) - 2, nullptr, [&validRequests](void *user, HttpRequest *req) {
            validRequests++;
        }, [](void *, std::string_view data) {

        }, [](void *) {

        });
    }
    auto stop = std::chrono::high_resolution_clock::now();

    std::cout << "Parsed " << validRequests << " in " << std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count() << "ms" << std::endl;
}

#endif // TESTS_H
