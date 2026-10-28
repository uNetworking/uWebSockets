#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

#define UWS_WITH_PROXY
#include "../src/ProxyParser.h"

/* PROXY v2 header for a TCP connection with the given family byte, address block and trailing TLVs */
static std::string proxyHeader(uint8_t fam, std::string addresses, std::string tlvs) {
    std::string header("\x0D\x0A\x0D\x0A\x00\x0D\x0A\x51\x55\x49\x54\x0A", 12);
    uint16_t length = (uint16_t) (addresses.length() + tlvs.length());
    header += (char) 0x21;
    header += (char) fam;
    header += (char) (length >> 8);
    header += (char) (length & 0xff);
    return header + addresses + tlvs;
}

int main() {
    /* ALPN TLV as sent by HAProxy's send-proxy-v2 for an h2 client */
    std::string alpn("\x01\x00\x02h2", 5);

    /* IPv6 with a TLV makes the payload longer than the address block (36 + 5 bytes) */
    std::string ipv6(36, '\0');
    ipv6[15] = 1;
    ipv6[31] = 2;
    ipv6[32] = 0x1f;
    ipv6[33] = 0x90;
    std::string v6 = proxyHeader(0x21, ipv6, alpn);
    std::string request = v6 + "GET / HTTP/1.1\r\n\r\n";

    uWS::ProxyParser v6Parser;
    auto [v6Done, v6Consumed] = v6Parser.parse(request);
    assert(v6Done);
    assert(v6Consumed == v6.length());
    assert(v6Parser.getSourceAddress() == std::string_view(ipv6.data(), 16));
    assert(v6Parser.getSourcePort() == 0x901f);

    /* IPv4 with the same TLV keeps working */
    std::string ipv4("\x7f\x00\x00\x01\x7f\x00\x00\x01\x1f\x90\x00\x50", 12);
    std::string v4 = proxyHeader(0x11, ipv4, alpn);

    uWS::ProxyParser v4Parser;
    auto [v4Done, v4Consumed] = v4Parser.parse(v4);
    assert(v4Done);
    assert(v4Consumed == v4.length());
    assert(v4Parser.getSourceAddress() == std::string_view(ipv4.data(), 4));

    /* A truncated payload still waits for more data */
    uWS::ProxyParser truncated;
    assert(!truncated.parse(std::string_view(v6.data(), v6.length() - 1)).first);
}
