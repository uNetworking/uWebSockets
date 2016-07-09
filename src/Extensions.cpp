#include "Extensions.h"

#ifdef NODEJS_WINDOWS
extern "C" {
int inflateInit2_(z_streamp strm, int windowBits, const char *version, int stream_size) {
    return Z_OK;
}

int deflate(z_streamp strm, int flush) {
    return Z_OK;
}

int deflateInit2_(z_streamp strm, int level, int method, int windowBits, int memLevel, int strategy, const char *version, int stream_size) {
    return Z_OK;
}

int inflate(z_streamp strm, int flush) {
    return Z_OK;
}

int inflateEnd(z_streamp strm) {
    return Z_OK;
}

int deflateEnd(z_streamp strm) {
    return Z_OK;
}

int deflateReset(z_streamp strm) {
    return Z_OK;
}

}
#endif

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

#ifdef NODEJS_WINDOWS
    perMessageDeflate = false;
#endif
}
