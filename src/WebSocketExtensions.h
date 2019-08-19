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

#ifndef UWS_WEBSOCKETEXTENSIONS_H
#define UWS_WEBSOCKETEXTENSIONS_H

#include <climits>
#include <string_view>

namespace uWS {

enum Options : unsigned int {
    NO_OPTIONS = 0,
    PERMESSAGE_DEFLATE = 1,
    SERVER_NO_CONTEXT_TAKEOVER = 2, // remove this
    CLIENT_NO_CONTEXT_TAKEOVER = 4, // remove this
    NO_DELAY = 8,
    SLIDING_DEFLATE_WINDOW = 16
};

enum ExtensionTokens {
    TOK_PERMESSAGE_DEFLATE = 1838,
    TOK_SERVER_NO_CONTEXT_TAKEOVER = 2807,
    TOK_CLIENT_NO_CONTEXT_TAKEOVER = 2783,
    TOK_SERVER_MAX_WINDOW_BITS = 2372,
    TOK_CLIENT_MAX_WINDOW_BITS = 2348
};

struct ExtensionsParser {
private:
    int *lastInteger = nullptr;

public:
    bool perMessageDeflate = false;
    bool serverNoContextTakeover = false;
    bool clientNoContextTakeover = false;
    int serverMaxWindowBits = 0;
    int clientMaxWindowBits = 0;

    int getToken(const char *&in, const char *stop) {
        while (in != stop && !isalnum(*in)) {
            in++;
        }

        /* Don't care more than this for now */
        static_assert(SHRT_MIN > INT_MIN, "Integer overflow fix is invalid for this platform, report this as a bug!");

        int hashedToken = 0;
        while (in != stop && (isalnum(*in) || *in == '-' || *in == '_')) {
            if (isdigit(*in)) {
                /* This check is a quick and incorrect fix for integer overflow
                 * in oss-fuzz but we don't care as it doesn't matter either way */
                if (hashedToken > SHRT_MIN && hashedToken < SHRT_MAX) {
                    hashedToken = hashedToken * 10 - (*in - '0');
                }
            } else {
                hashedToken += *in;
            }
            in++;
        }
        return hashedToken;
    }

    ExtensionsParser(const char *data, size_t length) {
        const char *stop = data + length;
        int token = 1;
        for (; token && token != TOK_PERMESSAGE_DEFLATE; token = getToken(data, stop));

        perMessageDeflate = (token == TOK_PERMESSAGE_DEFLATE);
        while ((token = getToken(data, stop))) {
            switch (token) {
            case TOK_PERMESSAGE_DEFLATE:
                return;
            case TOK_SERVER_NO_CONTEXT_TAKEOVER:
                serverNoContextTakeover = true;
                break;
            case TOK_CLIENT_NO_CONTEXT_TAKEOVER:
                clientNoContextTakeover = true;
                break;
            case TOK_SERVER_MAX_WINDOW_BITS:
                serverMaxWindowBits = 1;
                lastInteger = &serverMaxWindowBits;
                break;
            case TOK_CLIENT_MAX_WINDOW_BITS:
                clientMaxWindowBits = 1;
                lastInteger = &clientMaxWindowBits;
                break;
            default:
                if (token < 0 && lastInteger) {
                    *lastInteger = -token;
                }
                break;
            }
        }
    }
};

template <bool isServer>
struct ExtensionsNegotiator {
protected:
    int options;

public:
    ExtensionsNegotiator(int wantedOptions) {
        options = wantedOptions;
    }

    std::string generateOffer() {
        std::string extensionsOffer;
        if (options & Options::PERMESSAGE_DEFLATE) {
            extensionsOffer += "permessage-deflate";

            if (options & Options::CLIENT_NO_CONTEXT_TAKEOVER) {
                extensionsOffer += "; client_no_context_takeover";
            }

            /* It is questionable sending this improves anything */
            /*if (options & Options::SERVER_NO_CONTEXT_TAKEOVER) {
                extensionsOffer += "; server_no_context_takeover";
            }*/
        }

        return extensionsOffer;
    }

    void readOffer(std::string_view offer) {
        if (isServer) {
            ExtensionsParser extensionsParser(offer.data(), offer.length());
            if ((options & PERMESSAGE_DEFLATE) && extensionsParser.perMessageDeflate) {
                if (extensionsParser.clientNoContextTakeover || (options & CLIENT_NO_CONTEXT_TAKEOVER)) {
                    options |= CLIENT_NO_CONTEXT_TAKEOVER;
                }

                /* We leave this option for us to read even if the client did not send it */
                if (extensionsParser.serverNoContextTakeover) {
                    options |= SERVER_NO_CONTEXT_TAKEOVER;
                }/* else {
                    options &= ~SERVER_NO_CONTEXT_TAKEOVER;
                }*/
            } else {
                options &= ~PERMESSAGE_DEFLATE;
            }
        } else {
            // todo!
        }
    }

    int getNegotiatedOptions() {
        return options;
    }
};

}

#endif // UWS_WEBSOCKETEXTENSIONS_H
