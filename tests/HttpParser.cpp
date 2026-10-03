#include <iostream>
#include <cassert>
#include <string>

#include "../src/HttpParser.h"

int main() {
    /* RFC 9112 3.2: the request target is visible ASCII, so raw UTF-8 and DEL get a 400.
     * The long one puts the UTF-8 in a word with no space, where only the word scan can see it */
    for (const char *target : {"/caf\xc3\xa9", "/caf\xc3\xa9/long/enough/to/fill/a/word", "/\xc0\xaf", "/a\x7f"}) {
        std::string req = std::string("GET ") + target + " HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
        unsigned int length = (unsigned int) req.size();
        req.append(32, 'E');

        void *targetUser = (void *) 13;
        uWS::HttpParser targetParser;
        auto [targetErr, targetReturned] = targetParser.consumePostPadded(req.data(), length, targetUser, nullptr, [](void *s, uWS::HttpRequest *) -> void * {
            return s;
        }, [](void *s, std::string_view, bool) -> void * {
            return s;
        });

        if (targetReturned == targetUser) {
            std::cerr << "Target \"" << target << "\" expected 400" << std::endl;
            return 1;
        }
    }

    /* Parser needs at least 8 bytes post padding */
    unsigned char data[] = {0x47, 0x45, 0x54, 0x20, 0x2f, 0x20, 0x48, 0x54, 0x54, 0x50, 0x2f, 0x31, 0x2e, 0x31, 0xd, 0xa, 0x61, 0x73, 0x63, 0x69, 0x69, 0x3a, 0x20, 0x74, 0x65, 0x73, 0x74, 0xd, 0xa, 0x75, 0x74, 0x66, 0x38, 0x3a, 0x20, 0xd1, 0x82, 0xd0, 0xb5, 0xd1, 0x81, 0xd1, 0x82, 0xd, 0xa, 0x48, 0x6f, 0x73, 0x74, 0x3a, 0x20, 0x31, 0x32, 0x37, 0x2e, 0x30, 0x2e, 0x30, 0x2e, 0x31, 0xd, 0xa, 0x43, 0x6f, 0x6e, 0x6e, 0x65, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x3a, 0x20, 0x63, 0x6c, 0x6f, 0x73, 0x65, 0xd, 0xa, 0xd, 0xa, 'E', 'E', 'E', 'E', 'E', 'E', 'E', 'E'};
    int size = sizeof(data) - 8;
    void *user = nullptr;
    void *reserved = nullptr;

    uWS::HttpParser httpParser;

    auto [err, returnedUser] = httpParser.consumePostPadded((char *) data, size, user, reserved, [reserved](void *s, uWS::HttpRequest *httpRequest) -> void * {

        std::cout << httpRequest->getMethod() << std::endl;

        for (auto [key, value] : *httpRequest) {
            std::cout << key << ": " << value << std::endl;
        }

        /* Since we did proper whitespace trimming this thing is there, but empty */
        assert(httpRequest->getHeader("utf8").data());

        /* Return ok */
        return s;

    }, [](void *user, std::string_view data, bool fin) -> void * {

        /* Return ok */
        return user;

    });

    std::cout << "HTTP DONE" << std::endl;

    /* RFC 9112 7.1.2: trailer fields after the last chunk are skipped and the next request still parses.
     * A bare LF there is a 400, else the next request would be read as a trailer field */
    struct {
        const char *trailer;
        bool accept;
    } trailerCases[] = {
        {"X-Checksum: abc\r\n\r\n", true},
        {"\n", false},
        {"X-Checksum: abc\n\r\n", false},
    };

    for (auto &c : trailerCases) {
        std::string req = std::string("POST / HTTP/1.1\r\nHost: 127.0.0.1\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n0\r\n") + c.trailer
                          + "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
        unsigned int length = (unsigned int) req.size();
        req.append(32, 'E');

        int requests = 0;
        void *trailerUser = (void *) 13;
        uWS::HttpParser trailerParser;
        auto [trailerErr, trailerReturned] = trailerParser.consumePostPadded(req.data(), length, trailerUser, nullptr, [&requests](void *s, uWS::HttpRequest *) -> void * {
            requests++;
            return s;
        }, [](void *s, std::string_view, bool) -> void * {
            return s;
        });

        bool ok = c.accept ? (trailerReturned == trailerUser && requests == 2) : trailerReturned != trailerUser;
        if (!ok) {
            std::cerr << "Trailer \"" << c.trailer << "\" expected " << (c.accept ? "accept" : "400")
                      << ", got err=" << trailerErr << " requests=" << requests << std::endl;
            return 1;
        }
    }

    /* Issue 1941: accept Transfer-Encoding when the final coding is chunked (case-insensitive). */
    struct {
        const char *te;
        bool accept;
    } cases[] = {
        {"chunked", true},
        {"CHUNKED", true},
        {"gzip, chunked", true},
        {"gzip, CHUNKED", true},
        {"deflate, gzip, chunked", true},
        {"identity, chunked", true},
        {"gzip", false},
        {"chunked, gzip", false},
    };

    for (auto &c : cases) {
        std::string req = std::string("POST / HTTP/1.1\r\nHost: 127.0.0.1\r\nTransfer-Encoding: ") + c.te + "\r\n\r\n0\r\n\r\n";
        unsigned int length = (unsigned int) req.size();
        req.append(32, 'E');

        void *teUser = (void *) 13;
        uWS::HttpParser teParser;
        auto [teErr, teReturned] = teParser.consumePostPadded(req.data(), length, teUser, nullptr, [](void *s, uWS::HttpRequest *) -> void * {
            return s;
        }, [](void *s, std::string_view, bool) -> void * {
            return s;
        });

        bool accepted = teReturned == teUser;
        if (accepted != c.accept) {
            std::cerr << "Transfer-Encoding \"" << c.te << "\" expected " << (c.accept ? "accept" : "400")
                      << ", got err=" << teErr << std::endl;
            return 1;
        }
    }

    /* RFC 9112 3.2: a Host with a path, a query or a fragment is invalid */
    for (const char *host : {"localhost:8080/path", "localhost?a", "localhost#a"}) {
        std::string req = std::string("GET / HTTP/1.1\r\nHost: ") + host + "\r\n\r\n";
        unsigned int length = (unsigned int) req.size();
        req.append(32, 'E');

        void *hostUser = (void *) 13;
        uWS::HttpParser hostParser;
        auto [hostErr, hostReturned] = hostParser.consumePostPadded(req.data(), length, hostUser, nullptr, [](void *s, uWS::HttpRequest *) -> void * {
            return s;
        }, [](void *s, std::string_view, bool) -> void * {
            return s;
        });

        if (hostReturned == hostUser) {
            std::cerr << "Host \"" << host << "\" expected 400" << std::endl;
            return 1;
        }
    }

    /* RFC 9112 3.2.4: the asterisk form is only for OPTIONS, the app gets "*" as URL */
    for (std::string method : {"OPTIONS", "GET"}) {
        std::string req = method + " * HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
        unsigned int length = (unsigned int) req.size();
        req.append(32, 'E');

        std::string url;
        void *starUser = (void *) 13;
        uWS::HttpParser starParser;
        auto [starErr, starReturned] = starParser.consumePostPadded(req.data(), length, starUser, nullptr, [&url](void *s, uWS::HttpRequest *httpRequest) -> void * {
            url = httpRequest->getUrl();
            return s;
        }, [](void *s, std::string_view, bool) -> void * {
            return s;
        });

        bool accepted = starReturned == starUser && url == "*";
        if (accepted != (method == "OPTIONS")) {
            std::cerr << method << " * expected " << (method == "OPTIONS" ? "accept" : "400") << ", got err=" << starErr << std::endl;
            return 1;
        }
    }
}
