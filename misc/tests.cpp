#include "HttpParser.h"

#include <chrono>
#include <iostream>

// todo: random test of chunked http parsing of randomly generated requests
void testHttpParser() {

    char headers[] = "GET /hello.htm HTTP/1.1\r\n"
            "User-Agent: Mozilla/4.0 (compatible; MSIE5.01; Windows NT)\r\n"
            "Host: www.tutorialspoint.com\r\n"
            "Accept-Language: en-us\r\n"
            "Accept-Encoding: gzip, deflate\r\n"
            "Connection: Keep-Alive\r\n"
            "Content-length: 1048576\r\n\r\n";

    const int requestLength = sizeof(headers) - 1 + 1048576;
    char *request = (char *) malloc(requestLength + 32);
    memset(request, 0, requestLength);
    memcpy(request, headers, sizeof(headers) - 1);

    char *data = (char *) malloc(requestLength * 10);
    int length = requestLength * 10;

    int maxChunkSize = 10000;
    char *paddedBuffer = (char *) malloc(maxChunkSize + 32);

    // dela upp dessa 10 i 5 segment
    HttpParser httpParser;
    int validRequests = 0, numDataEmits = 0, numChunks = 0;
    size_t dataBytes = 0;

    for (int i = 0; i < 10; i++) {
        memcpy(data + requestLength * i, request, requestLength);
    }

    for (int j = 0; j < 1000; j++) {
        for (int currentOffset = 0; currentOffset != length; ) {

            int chunkSize = rand() % 10000;
            if (currentOffset + chunkSize > length) {
                chunkSize = length - currentOffset;
            }

            memcpy(paddedBuffer, data + currentOffset, chunkSize);

            httpParser.consumePostPadded(paddedBuffer, chunkSize, nullptr, [&validRequests](void *user, HttpRequest *req) {
                validRequests++;

                if (req->getUrl() != "/hello.htm") {
                    std::cout << "WRONG URL!" << std::endl;
                    exit(-1);
                }

            }, [&dataBytes, &numDataEmits](void *, std::string_view data) {
                numDataEmits++;
                dataBytes += data.length();
            }, [](void *) {

                std::cout << "Error!" << std::endl;
                return;
            });

            numChunks++;

            currentOffset += chunkSize;
        }
    }

    std::cout << "validRequests: " << validRequests << std::endl;
    std::cout << "Data bytes: " << dataBytes << std::endl;
    std::cout << "Data emits: " << numDataEmits << std::endl;
    std::cout << "Chunks parsed: " << numChunks << std::endl;

    validRequests = 0;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000000; i++) {
        httpParser.consumePostPadded(request, requestLength, nullptr, [&validRequests](void *user, HttpRequest *req) {
            validRequests++;
        }, [](void *, std::string_view data) {

        }, [](void *) {

        });
    }
    auto stop = std::chrono::high_resolution_clock::now();

    std::cout << "Parsed " << validRequests << " in " << std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count() << "ms" << std::endl;
}

int main() {
	testHttpParser();
}
