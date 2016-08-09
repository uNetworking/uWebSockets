#include "Extensions.h"

int ExtensionsParser::getToken(const char **in)
{
    while (!isalnum(**in) && **in != '\0') {
        (*in)++;
    }

    int hashedToken = 0;
    while (isalnum(**in) || **in == '-' || **in == '_') {
        if (isdigit(**in)) {
            hashedToken = hashedToken * 10 - (**in - '0');
        } else {
            hashedToken += **in;
        }
        (*in)++;
    }
    return hashedToken;
}

ExtensionsParser::ExtensionsParser(const char *in)
{
    int token = 1;
    for (; token && token != PERMESSAGE_DEFLATE; token = getToken(&in));

    perMessageDeflate = (token == PERMESSAGE_DEFLATE);
    while ((token = getToken(&in))) {
        switch (token) {
        case PERMESSAGE_DEFLATE:
            return;
        case SERVER_NO_CONTEXT_TAKEOVER:
            serverNoContextTakeover = true;
            break;
        case CLIENT_NO_CONTEXT_TAKEOVER:
            clientNoContextTakeover = true;
            break;
        case SERVER_MAX_WINDOW_BITS:
            serverMaxWindowBits = 1;
            lastInteger = &serverMaxWindowBits;
            break;
        case CLIENT_MAX_WINDOW_BITS:
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
