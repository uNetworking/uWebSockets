/*
 * Authored by Alex Hultman, 2018-2020.
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

/* Implements the multipart protocol. Builds atop parts of our common http parser (not yet refactored that way). */
/* https://www.w3.org/Protocols/rfc1341/7_2_Multipart.html */

#ifndef UWS_MULTIPART_H
#define UWS_MULTIPART_H

#include "MessageParser.h"

#include <string_view>
#include <cstring>
#include <utility>

namespace uWS {

    struct MultipartParser {

        /* 2 chars of hyphen + 1 - 70 chars of boundary */
        char prependedBoundaryBuffer[72];
        std::string_view prependedBoundary;
        std::string_view remainingBody;
        bool first = true;

        /* I think it is more than sane to limit this to 10 per part */
        //static const int MAX_HEADERS = 10;

        /* Construct the parser based on contentType (reads boundary) */
        MultipartParser(std::string_view contentType) {

            /* We expect the form "multipart/something;somethingboundary=something" */
            if (contentType.length() < 10 || contentType.substr(0, 10) != "multipart/") {
                return;
            }

            /* For now we simply guess boundary will lie between = and end. This is not entirely
            * standards compliant as boundary may be expressed with or without " and spaces */
            auto equalToken = contentType.find('=', 10);
            if (equalToken != std::string_view::npos) {

                /* Boundary must be less than or equal to 70 chars yet 1 char or longer */
                std::string_view boundary = contentType.substr(equalToken + 1);
                if (!boundary.length() || boundary.length() > 70) {
                    /* Invalid size */
                    return;
                }

                /* Prepend it with two hyphens */
                prependedBoundaryBuffer[0] = prependedBoundaryBuffer[1] = '-';
                memcpy(&prependedBoundaryBuffer[2], boundary.data(), boundary.length());

                prependedBoundary = {prependedBoundaryBuffer, boundary.length() + 2};
            }
        }

        /* Is this even a valid multipart request? */
        bool isValid() {
            return prependedBoundary.length() != 0;
        }

        /* Set the body once, before getting any parts */
        void setBody(std::string_view body) {
            remainingBody = body;
        }

        /* Parse out the next part's data, filling the headers. Returns empty part on end or error */
        std::string_view getNextPart(std::pair<std::string_view, std::string_view> *headers) {

            /* The remaining two hyphens should be shorter than the boundary */
            if (remainingBody.length() < prependedBoundary.length()) {
                /* We are done now */
                return {};
            }

            if (first) {
                auto nextBoundary = remainingBody.find(prependedBoundary);
                if (nextBoundary == std::string_view::npos) {
                    /* Cannot parse */
                    return {};
                }

                /* Toss away boundary and anything before it */
                remainingBody.remove_prefix(nextBoundary + prependedBoundary.length());
                first = false;
            }

            auto nextEndBoundary = remainingBody.find(prependedBoundary);
            if (nextEndBoundary == std::string_view::npos) {
                /* Cannot parse (or simply done) */
                return {};
            }

            std::string_view part = remainingBody.substr(0, nextEndBoundary);
            remainingBody.remove_prefix(nextEndBoundary + prependedBoundary.length());

            /* Also strip rn before and rn after the part */
            if (part.length() < 4) {
                /* Cannot strip */
                return {};
            }
            part.remove_prefix(2);
            part.remove_suffix(2);

            /* We are allowed to post pad like this because we know the boundary is at least 2 bytes */
            /* This makes parsing a second pass invalid, so you can only iterate over parts once */
            memset((char *) part.data() + part.length(), '\r', 1);

            /* For this to be a valid part, we need to consume at least 4 bytes (\r\n\r\n) */
            int consumed = getHeaders((char *) part.data(), (char *) part.data() + part.length(), headers);

            if (!consumed) {
                /* This is an invalid part */
                return {};
            }

            /* Strip away the headers from the part body data */
            part.remove_prefix(consumed);

            /* Now pass whatever is remaining of the part */
            return part;
        }
    };

}

#endif