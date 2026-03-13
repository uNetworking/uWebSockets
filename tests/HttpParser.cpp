#include <iostream>
#include <cassert>

#include "../src/HttpParser.h"

int main() {
    /* Parser needs at least 8 bytes post padding */
    unsigned char data[] = {0x47, 0x45, 0x54, 0x20, 0x2f, 0x20, 0x48, 0x54, 0x54, 0x50, 0x2f, 0x31, 0x2e, 0x31, 0xd, 0xa, 0x61, 0x73, 0x63, 0x69, 0x69, 0x3a, 0x20, 0x74, 0x65, 0x73, 0x74, 0xd, 0xa, 0x75, 0x74, 0x66, 0x38, 0x3a, 0x20, 0xd1, 0x82, 0xd0, 0xb5, 0xd1, 0x81, 0xd1, 0x82, 0xd, 0xa, 0x48, 0x6f, 0x73, 0x74, 0x3a, 0x20, 0x31, 0x32, 0x37, 0x2e, 0x30, 0x2e, 0x30, 0x2e, 0x31, 0xd, 0xa, 0x43, 0x6f, 0x6e, 0x6e, 0x65, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x3a, 0x20, 0x63, 0x6c, 0x6f, 0x73, 0x65, 0xd, 0xa, 0xd, 0xa, 'E', 'E', 'E', 'E', 'E', 'E', 'E', 'E'};
    int size = sizeof(data) - 8;
    void *user = nullptr;
    void *reserved = nullptr;

    uWS::HttpParser httpParser;

    auto [err, returnedUser] = httpParser.consumePostPadded((char *) data, size, user, reserved, [reserved, &httpParser](void *s, uWS::HttpRequest *httpRequest) -> void * {

        std::cout << httpRequest->getMethod() << std::endl;

        for (auto [key, value] : *httpRequest) {
            std::cout << key << ": " << value << std::endl;
        }

        /* Since we did proper whitespace trimming this thing is there, but empty */
        assert(httpRequest->getHeader("utf8").data());

        /* maxRemainingBodyLength() must be 0 for GET requests (no body) when called inside requestHandler */
        assert(httpParser.maxRemainingBodyLength() == 0);

        /* Return ok */
        return s;

    }, [](void *user, std::string_view data, bool fin) -> void * {

        /* Return ok */
        return user;

    });

    std::cout << "HTTP DONE" << std::endl;

    /* Test that maxRemainingBodyLength() is set correctly before requestHandler for a POST with content-length */
    {
        /* POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhello + 8 bytes padding */
        const char postData[] = "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhelloEEEEEEEE";
        int postSize = (int)(sizeof(postData) - 1 - 8);
        uWS::HttpParser postParser;
        uint64_t bodyLengthInHandler = UINT64_MAX;
        uint64_t bodyLengthInDataHandler = UINT64_MAX;

        postParser.consumePostPadded((char *) postData, postSize, user, reserved, [&postParser, &bodyLengthInHandler](void *s, uWS::HttpRequest *httpRequest) -> void * {
            bodyLengthInHandler = postParser.maxRemainingBodyLength();
            return s;
        }, [&postParser, &bodyLengthInDataHandler](void *user, std::string_view data, bool fin) -> void * {
            /* maxRemainingBodyLength() must be decremented before dataHandler is called */
            bodyLengthInDataHandler = postParser.maxRemainingBodyLength();
            return user;
        });

        /* maxRemainingBodyLength() must return content-length (5) when called inside requestHandler */
        assert(bodyLengthInHandler == 5);
        std::cout << "POST content-length in requestHandler test PASS" << std::endl;
        /* maxRemainingBodyLength() must be 0 after all body is consumed (decremented before dataHandler) */
        assert(bodyLengthInDataHandler == 0);
        std::cout << "POST content-length in dataHandler test PASS" << std::endl;
    }

}