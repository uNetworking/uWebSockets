#ifndef EXTENSIONS_H
#define EXTENSIONS_H

#include <string>
#include <zlib.h>

enum ExtensionTokens {
    PERMESSAGE_DEFLATE = 1838,
    SERVER_NO_CONTEXT_TAKEOVER = 2807,
    CLIENT_NO_CONTEXT_TAKEOVER = 2783,
    SERVER_MAX_WINDOW_BITS = 2372,
    CLIENT_MAX_WINDOW_BITS = 2348
};

class ExtensionsParser {
private:
    int *lastInteger = nullptr;

public:
    bool perMessageDeflate = false;
    bool serverNoContextTakeover = false;
    bool clientNoContextTakeover = false;
    int serverMaxWindowBits = 0;
    int clientMaxWindowBits = 0;

    int getToken(const char **in);
    ExtensionsParser(const char *in);
};

struct PerMessageDeflate {
    z_stream readStream, writeStream;
    bool compressedFrame;
    bool serverNoContextTakeover = false;
    bool clientNoContextTakeover = false;

    PerMessageDeflate(ExtensionsParser &extensionsParser, int options, std::string &response) : readStream({}), writeStream({})
    {
        response = "Sec-WebSocket-Extensions: permessage-deflate";
        if ((options & SERVER_NO_CONTEXT_TAKEOVER) || extensionsParser.serverNoContextTakeover) {
            response += "; server_no_context_takeover";
            serverNoContextTakeover = true;
        }
        if ((options & CLIENT_NO_CONTEXT_TAKEOVER) || extensionsParser.clientNoContextTakeover) {
            response += "; client_no_context_takeover";
            clientNoContextTakeover = true;
        }

        inflateInit2(&readStream, -15);
    }

    ~PerMessageDeflate()
    {
        inflateEnd(&readStream);
    }

    void setInput(char *src, size_t srcLength) {
        readStream.next_in = (unsigned char *) src;
        readStream.avail_in = srcLength;
    }

    size_t inflate(char *dst, size_t dstLength) {
        if (!readStream.avail_in) {
            return dstLength;
        }
        readStream.next_out = (unsigned char *) dst;
        readStream.avail_out = dstLength;
        int err = ::inflate(&readStream, Z_NO_FLUSH);
        if (err != Z_STREAM_END && err != Z_OK) {
            throw err;
        }
        return readStream.avail_out;
    }
};

#endif // EXTENSIONS_H
