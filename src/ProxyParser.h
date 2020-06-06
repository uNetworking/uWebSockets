//

// implements PROXY v2 protocol

#ifndef PROXY_PARSER
#define PROXY_PARSER

#ifdef WITH_PROXY

struct proxy_hdr_v2 {
    uint8_t sig[12];  /* hex 0D 0A 0D 0A 00 0D 0A 51 55 49 54 0A */
    uint8_t ver_cmd;  /* protocol version and command */
    uint8_t fam;      /* protocol family and address */
    uint16_t len;     /* number of following bytes part of the header */
};

/* Byte swap for little-endian systems */
template <typename T>
T _cond_byte_swap(T value) {
    uint32_t endian_test = 1;
    if (*((char *)&endian_test)) {
        union {
            T i;
            uint8_t b[sizeof(T)];
        } src = { value }, dst;

        for (unsigned int i = 0; i < sizeof(value); i++) {
            dst.b[i] = src.b[sizeof(value) - 1 - i];
        }

        return dst.i;
    }
    return value;
}

struct ProxyParser {
private:
    unsigned char sourceIp[16];
    unsigned char destIp[16];
    uint16_t sourcePort, destPort;

public:
    /* Returns 4 or 16 bytes */
    std::string_view getSourceIP() {
        return {"hello", 5};
    }

    /* Returns [done, consumed] where done = false on failure */
    std::pair<bool, unsigned int> parse(std::string_view data) {

        /* We require at least one byte to be done */
        if (!data.length()) {
            return {false, 0};
        }

        /* HTTP does not start with \r, but PROXY always does */
        if (data[0] != '\r') {
            //printf("This is HTTP\n");
            /* This is HTTP, so be done */
            return {true, 0};
        }

        /* We assume we are parsing PROXY V2 here */
        printf("This is PROXY v2\n");

        /* We require 16 bytes here */
        if (data.length() < 16) {
            return {false, 0};
        }

        struct proxy_hdr_v2 header;
        memcpy(&header, data.data(), 16);

        if (memcmp(header.sig, "\x0D\x0A\x0D\x0A\x00\x0D\x0A\x51\x55\x49\x54\x0A", 12)) {
            /* This is not PROXY protocol at all */
            return {false, 0};
        }

        printf("Version: %d\n", (header.ver_cmd & 0xf0) >> 4);
        printf("Command: %d\n", (header.ver_cmd & 0x0f));

        uint16_t hostLength = _cond_byte_swap<uint16_t>(header.len);

        printf("Length: %d\n", hostLength);

        printf("Family: %d\n", (header.fam & 0xf0) >> 4);
        printf("Transport: %d\n", (header.fam & 0x0f));

        return {true, 16 + hostLength};
    }

};

#endif

#endif