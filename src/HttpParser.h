/*
 * Authored by Alex Hultman, 2018-2019.
 * Intellectual property of third-party.

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 *     http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef HTTPPARSER_H
#define HTTPPARSER_H

/* The HTTP parser is an independent module subject to unit testing / fuzz testing */

#include <string>
#include <functional>
#include <cstring>
#include <algorithm>

#include "f2/function2.hpp"

namespace uWS {

class HttpRequest {

    friend class HttpParser;

private:
    const static int MAX_HEADERS = 50;
    struct Header {
        std::string_view key, value;
    } headers[MAX_HEADERS];
    int querySeparator;
    bool didYield;

    std::pair<int, std::string_view *> currentParameters;

public:
    bool getYield() {
        return didYield;
    }

    /* If you do not want to handle this route */
    void setYield(bool yield) {
        didYield = yield;
    }

    std::string_view getHeader(std::string_view header) {
        for (Header *h = headers; (++h)->key.length(); ) {
            if (h->key.length() == header.length() && !strncmp(h->key.data(), header.data(), header.length())) {
                return h->value;
            }
        }
        return std::string_view(nullptr, 0);
    }

    // todo: implement this
    /*int getHeader(std::string_view header) {
        return 0;
    }*/

    std::string_view getUrl() {
        return std::string_view(headers->value.data(), querySeparator);
    }

    std::string_view getMethod() {
        return std::string_view(headers->key.data(), headers->key.length());
    }

    std::string_view getQuery() {
        return std::string_view(headers->value.data() + querySeparator, headers->value.length() - querySeparator);
    }

    void setParameters(std::pair<int, std::string_view *> parameters) {
        currentParameters = parameters;
    }

    std::string_view getParameter(unsigned int index) {
        if (currentParameters.first < index) {
            return {};
        } else {
            return currentParameters.second[index];
        }
    }

};

class HttpParser {

private:
    std::string fallback;
    int remainingStreamingBytes = 0;

    const size_t MAX_FALLBACK_SIZE = 1024 * 4;

    static unsigned int toUnsignedInteger(std::string_view str) {
        int unsignedIntegerValue = 0;
        for (unsigned char c : str) {
            unsignedIntegerValue = unsignedIntegerValue * 10 + (c - '0');
        }
        return unsignedIntegerValue;
    }

    static unsigned int getHeaders(char *postPaddedBuffer, char *end, struct HttpRequest::Header *headers) {
        char *preliminaryKey, *preliminaryValue, *start = postPaddedBuffer;

        for (unsigned int i = 0; i < HttpRequest::MAX_HEADERS; i++) {
            for (preliminaryKey = postPaddedBuffer; (*postPaddedBuffer != ':') & (*postPaddedBuffer > 32); *(postPaddedBuffer++) |= 32);
            if (*postPaddedBuffer == '\r') {
                if ((postPaddedBuffer != end) & (postPaddedBuffer[1] == '\n') & (i > 0)) {
                    headers->key = std::string_view(nullptr, 0);
                    return (postPaddedBuffer + 2) - start;
                } else {
                    return 0;
                }
            } else {
                headers->key = std::string_view(preliminaryKey, (size_t) (postPaddedBuffer - preliminaryKey));
                for (postPaddedBuffer++; (*postPaddedBuffer == ':' || *postPaddedBuffer < 33) && *postPaddedBuffer != '\r'; postPaddedBuffer++);
                preliminaryValue = postPaddedBuffer;
                postPaddedBuffer = (char *) memchr(postPaddedBuffer, '\r', end - postPaddedBuffer);
                if (postPaddedBuffer && postPaddedBuffer[1] == '\n') {
                    headers->value = std::string_view(preliminaryValue, (size_t) (postPaddedBuffer - preliminaryValue));
                    postPaddedBuffer += 2;
                    headers++;
                } else {
                    return 0;
                }
            }
        }
        return 0;
    }

    // the only caller of getHeaders
    template <int CONSUME_MINIMALLY>
    std::pair<int, void *> fenceAndConsumePostPadded(char *data, int length, void *user, HttpRequest *req, fu2::unique_function<void *(void *, HttpRequest *)> &requestHandler, fu2::unique_function<void *(void *, std::string_view, bool)> &dataHandler) {
        int consumedTotal = 0;
        data[length] = '\r';

        for (int consumed; length && (consumed = getHeaders(data, data + length, req->headers)); ) {
            data += consumed;
            length -= consumed;
            consumedTotal += consumed;

            req->headers->value = std::string_view(req->headers->value.data(), std::max<int>(0, req->headers->value.length() - 9));

            // querySeparator is untested, todo: go through this
            const char *querySeparatorPtr = (const char *) memchr(req->headers->value.data(), '?', req->headers->value.length());
            req->querySeparator = (querySeparatorPtr ? querySeparatorPtr : req->headers->value.data() + req->headers->value.length()) - req->headers->value.data();

            // this one should return socket and exit on closed
            // what happens with data left for websockets?
            void *returnedUser = requestHandler(user, req);
            if (returnedUser != user) {
                // upgraded socket, or otherwise broken

                // return pair of consumed and user
                return {consumedTotal, returnedUser};
            }

            // do not check this for GET!

            // todo: also support reading chunked streams
            std::string_view contentLengthString = req->getHeader("content-length");
            if (contentLengthString.length()) {
                remainingStreamingBytes = toUnsignedInteger(contentLengthString);

                if (!CONSUME_MINIMALLY) {
                    int emittable = std::min(remainingStreamingBytes, length);
                    dataHandler(user, std::string_view(data, emittable), emittable == remainingStreamingBytes);
                    remainingStreamingBytes -= emittable;

                    data += emittable;
                    length -= emittable;
                    consumedTotal += emittable;
                }
            }

            if (CONSUME_MINIMALLY) {
                break;
            }
        }
        return {consumedTotal, user};
    }

public:

    // todo: what can we do with the socket inside the handlers? we need to check on return from any handler if we closed or terminated or upgraded the socket
    void *consumePostPadded(char *data, int length, void *user, fu2::unique_function<void *(void *, HttpRequest *)> &&requestHandler, fu2::unique_function<void *(void *, std::string_view, bool)> &&dataHandler, fu2::unique_function<void *(void *)> &&errorHandler) {

        HttpRequest req;

        if (remainingStreamingBytes) {

            // this is exactly the same as below!
            if (remainingStreamingBytes >= length) {
                void *returnedUser = dataHandler(user, std::string_view(data, length), remainingStreamingBytes == length);
                remainingStreamingBytes -= length;
                return returnedUser;
            } else {
                void *returnedUser = dataHandler(user, std::string_view(data, remainingStreamingBytes), true);

                data += remainingStreamingBytes;
                length -= remainingStreamingBytes;

                remainingStreamingBytes = 0;

                if (returnedUser != user) {
                    return returnedUser;
                }
            }

        } else if (fallback.length()) {
            int had = fallback.length();

            int maxCopyDistance = std::min(MAX_FALLBACK_SIZE - fallback.length(), (size_t) length);

            fallback.reserve(maxCopyDistance + 32); // padding should be same as libus
            fallback.append(data, maxCopyDistance);

            // break here on break
            std::pair<int, void *> consumed = fenceAndConsumePostPadded<true>(fallback.data(), fallback.length(), user, &req, requestHandler, dataHandler);
            if (consumed.second != user) {
                return consumed.second;
            }

            if (consumed.first) {

                fallback.clear();

                data += consumed.first - had;
                length -= consumed.first - had;

                if (remainingStreamingBytes) {
                    // this is exactly the same as above!
                    if (remainingStreamingBytes >= length) {
                        void *returnedUser = dataHandler(user, std::string_view(data, length), remainingStreamingBytes == length);
                        remainingStreamingBytes -= length;
                        return returnedUser;
                    } else {
                        void *returnedUser = dataHandler(user, std::string_view(data, remainingStreamingBytes), true);

                        data += remainingStreamingBytes;
                        length -= remainingStreamingBytes;

                        remainingStreamingBytes = 0;

                        if (returnedUser != user) {
                            return returnedUser;
                        }
                    }
                }

            } else {
                if (fallback.length() == MAX_FALLBACK_SIZE) {
                    // you don't really need error handler, just return something strange!
                    // we could have it return a constant pointer to denote error!
                    return errorHandler(user);
                }
                return user;
            }
        }

        std::pair<int, void *> consumed = fenceAndConsumePostPadded<false>(data, length, user, &req, requestHandler, dataHandler);
        if (consumed.second != user) {
            return consumed.second;
        }

        data += consumed.first;
        length -= consumed.first;

        if (length) {
            if (length < MAX_FALLBACK_SIZE) {
                fallback.append(data, length);
            } else {
                return errorHandler(user);
            }
        }

        // added for now
        return user;
    }
};

}

#endif // HTTPPARSER_H
