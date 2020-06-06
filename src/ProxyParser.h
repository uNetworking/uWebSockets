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

union proxy_addr {
    struct {        /* for TCP/UDP over IPv4, len = 12 */
        uint32_t src_addr;
        uint32_t dst_addr;
        uint16_t src_port;
        uint16_t dst_port;
    } ipv4_addr;
    struct {        /* for TCP/UDP over IPv6, len = 36 */
            uint8_t  src_addr[16];
            uint8_t  dst_addr[16];
            uint16_t src_port;
            uint16_t dst_port;
    } ipv6_addr;
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
    union proxy_addr addr = {};
    uint8_t family = 0;

public:
    /* Returns 4 or 16 bytes source address */
    std::string_view getSourceAddress() {
        if ((family & 0xf0) >> 4 == 1) {
            /* Family 1 is INET4 */
            return {(char *) &addr.ipv4_addr.src_addr, 4};
        } else {
            /* Family 2 is INET6 */
            return {(char *) &addr.ipv6_addr.src_addr, 16};
        }
    }

    /* Returns [done, consumed] where done = false on failure */
    std::pair<bool, unsigned int> parse(std::string_view data) {

        /* We require at least one byte to be done */
        if (!data.length()) {
            return {false, 0};
        }

        /* HTTP does not start with \r, but PROXY always does */
        if (data[0] != '\r') {
            /* This is HTTP, so be done */
            return {true, 0};
        }

        /* We assume we are parsing PROXY V2 here */

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

        /* If we have version 2 */

        printf("Version: %d\n", (header.ver_cmd & 0xf0) >> 4);
        printf("Command: %d\n", (header.ver_cmd & 0x0f));

        uint16_t hostLength = _cond_byte_swap<uint16_t>(header.len);

        if (data.length() < 16 + hostLength) {
            return {false, 0};
        }

        printf("Length: %d\n", hostLength);

        printf("Family: %d\n", (header.fam & 0xf0) >> 4);
        printf("Transport: %d\n", (header.fam & 0x0f));

        family = header.fam;

        // copy source ip
        memcpy(&addr, data.data() + 16, hostLength);



        return {true, 16 + hostLength};
    }

};

#endif

#endif