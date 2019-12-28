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

#ifndef UWS_HTTPPARSER_H
#define UWS_HTTPPARSER_H

// todo: HttpParser is in need of a few clean-ups and refactorings

/* The HTTP parser is an independent module subject to unit testing / fuzz testing */

#include <string>
#include <cstring>
#include <algorithm>
#include "f2/function2.hpp"

namespace uWS {

/* We require at least this much post padding */
static const int MINIMUM_HTTP_POST_PADDING = 32;

struct HttpRequest {

    friend struct HttpParser;

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

    /* Iteration over headers (key, value) */
    struct HeaderIterator {
        Header *ptr;

        bool operator!=(const HeaderIterator &other) const {
            /* Comparison with end is a special case */
            if (ptr != other.ptr) {
                return other.ptr || ptr->key.length();
            }
            return false;
        }

        HeaderIterator &operator++() {
            ptr++;
            return *this;
        }

        std::pair<std::string_view, std::string_view> operator*() const {
            return {ptr->key, ptr->value};
        }
    };

    HeaderIterator begin() {
        return {headers + 1};
    }

    HeaderIterator end() {
        return {nullptr};
    }

    /* If you do not want to handle this route */
    void setYield(bool yield) {
        didYield = yield;
    }

    std::string_view getHeader(std::string_view lowerCasedHeader) {
        for (Header *h = headers; (++h)->key.length(); ) {
            if (h->key.length() == lowerCasedHeader.length() && !strncmp(h->key.data(), lowerCasedHeader.data(), lowerCasedHeader.length())) {
                return h->value;
            }
        }
        return std::string_view(nullptr, 0);
    }

    std::string_view getUrl() {
        return std::string_view(headers->value.data(), querySeparator);
    }

    std::string_view getMethod() {
        return std::string_view(headers->key.data(), headers->key.length());
    }

    std::string_view getQuery() {
        if (querySeparator < (int) headers->value.length()) {
            /* Strip the initial ? */
            return std::string_view(headers->value.data() + querySeparator + 1, headers->value.length() - querySeparator - 1);
        } else {
            return std::string_view(nullptr, 0);
        }
    }

    void setParameters(std::pair<int, std::string_view *> parameters) {
        currentParameters = parameters;
    }

    std::string_view getParameter(unsigned int index) {
        if (currentParameters.first < (int) index) {
            return {};
        } else {
            return currentParameters.second[index];
        }
    }

};

struct HttpParser {

private:
    std::string fallback;
    unsigned int remainingStreamingBytes = 0;

    const size_t MAX_FALLBACK_SIZE = 1024 * 4;

    static unsigned int toUnsignedInteger(std::string_view str) {
        unsigned int unsignedIntegerValue = 0;
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
                    return (unsigned int) ((postPaddedBuffer + 2) - start);
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

            req->headers->value = std::string_view(req->headers->value.data(), std::max<int>(0, (int) req->headers->value.length() - 9));

            /* Parse query */
            const char *querySeparatorPtr = (const char *) memchr(req->headers->value.data(), '?', req->headers->value.length());
            req->querySeparator = (int) ((querySeparatorPtr ? querySeparatorPtr : req->headers->value.data() + req->headers->value.length()) - req->headers->value.data());

            /* If returned socket is not what we put in we need
             * to break here as we either have upgraded to
             * WebSockets or otherwise closed the socket. */
            void *returnedUser = requestHandler(user, req);
            if (returnedUser != user) {
                /* We are upgraded to WebSocket or otherwise broken */
                return {consumedTotal, returnedUser};
            }

            // todo: do not check this for GET (get should not have a body)
            // todo: also support reading chunked streams
            std::string_view contentLengthString = req->getHeader("content-length");
            if (contentLengthString.length()) {
                remainingStreamingBytes = toUnsignedInteger(contentLengthString);

                if (!CONSUME_MINIMALLY) {
                    unsigned int emittable = std::min<unsigned int>(remainingStreamingBytes, length);
                    dataHandler(user, std::string_view(data, emittable), emittable == remainingStreamingBytes);
                    remainingStreamingBytes -= emittable;

                    data += emittable;
                    length -= emittable;
                    consumedTotal += emittable;
                }
            } else {
                /* Still emit an empty data chunk to signal no data */
                dataHandler(user, {}, true);
            }

            if (CONSUME_MINIMALLY) {
                break;
            }
        }
        return {consumedTotal, user};
    }

public:

    /* We do this to prolong the validity of parsed headers by keeping only the fallback buffer alive */
    std::string &&salvageFallbackBuffer() {
        return std::move(fallback);
    }

    void *consumePostPadded(char *data, int length, void *user, fu2::unique_function<void *(void *, HttpRequest *)> &&requestHandler, fu2::unique_function<void *(void *, std::string_view, bool)> &&dataHandler, fu2::unique_function<void *(void *)> &&errorHandler) {

        HttpRequest req;

        if (remainingStreamingBytes) {

            // this is exactly the same as below!
            // todo: refactor this
            if (remainingStreamingBytes >= (unsigned int) length) {
                void *returnedUser = dataHandler(user, std::string_view(data, length), remainingStreamingBytes == (unsigned int) length);
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
            int had = (int) fallback.length();

            int maxCopyDistance = (int) std::min(MAX_FALLBACK_SIZE - fallback.length(), (size_t) length);

            /* We don't want fallback to be short string optimized, since we want to move it */
            fallback.reserve(fallback.length() + maxCopyDistance + std::max<int>(MINIMUM_HTTP_POST_PADDING, sizeof(std::string)));
            fallback.append(data, maxCopyDistance);

            // break here on break
            std::pair<int, void *> consumed = fenceAndConsumePostPadded<true>(fallback.data(), (int) fallback.length(), user, &req, requestHandler, dataHandler);
            if (consumed.second != user) {
                return consumed.second;
            }

            if (consumed.first) {

                fallback.clear();

                data += consumed.first - had;
                length -= consumed.first - had;

                if (remainingStreamingBytes) {
                    // this is exactly the same as above!
                    if (remainingStreamingBytes >= (unsigned int) length) {
                        void *returnedUser = dataHandler(user, std::string_view(data, length), remainingStreamingBytes == (unsigned int) length);
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
                    // note: you don't really need error handler, just return something strange!
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
            if ((unsigned int) length < MAX_FALLBACK_SIZE) {
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

#endif // UWS_HTTPPARSER_H
