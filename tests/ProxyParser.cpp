#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

#define UWS_WITH_PROXY
#include "../src/ProxyParser.h"

static std::string header(uint8_t verCmd, uint8_t fam, uint16_t len) {
    std::string h("\x0D\x0A\x0D\x0A\x00\x0D\x0A\x51\x55\x49\x54\x0A", 12);
    h += (char) verCmd;
    h += (char) fam;
    h += (char) (len >> 8);
    h += (char) (len & 0xff);
    for (uint16_t i = 0; i < len; i++) {
        h += (char) (i + 1);
    }
    return h;
}

int main() {

    {
        uWS::ProxyParser pp;
        std::string h = header(0x21, 0x11, 12);
        assert(pp.parse(h) == std::make_pair(true, 28u));
        assert(pp.getSourceAddress() == std::string_view("\x01\x02\x03\x04", 4));
    }

    {
        uWS::ProxyParser pp;
        std::string h = header(0x21, 0x21, 36);
        assert(pp.parse(h) == std::make_pair(true, 52u));
        assert(pp.getSourceAddress().length() == 16);
    }

    {
        uWS::ProxyParser pp;
        std::string h = header(0x21, 0x11, 40);
        assert(pp.parse(h) == std::make_pair(true, 56u));
        assert(pp.getSourceAddress() == std::string_view("\x01\x02\x03\x04", 4));
    }

    {
        uWS::ProxyParser pp;
        std::string h = header(0x20, 0x11, 12);
        assert(pp.parse(h) == std::make_pair(true, 28u));
        assert(pp.getSourceAddress().empty());
    }

    {
        uWS::ProxyParser pp;
        assert(pp.parse(header(0x22, 0x11, 12)).first == false);
    }

    {
        uWS::ProxyParser pp;
        assert(pp.parse(header(0x21, 0x41, 12)).first == false);
        assert(pp.parse(header(0x21, 0x01, 0)).first == false);
        assert(pp.parse(header(0x21, 0x13, 12)).first == false);
    }

    {
        uWS::ProxyParser pp;
        assert(pp.parse(header(0x21, 0x11, 0)).first == false);
        assert(pp.parse(header(0x21, 0x21, 12)).first == false);
    }

    {
        uWS::ProxyParser pp;
        std::string h = header(0x21, 0x00, 0);
        assert(pp.parse(h) == std::make_pair(true, 16u));
        assert(pp.getSourceAddress().empty());
    }

    {
        uWS::ProxyParser pp;
        std::string h = header(0x21, 0x31, 216);
        assert(pp.parse(h) == std::make_pair(true, 232u));
        assert(pp.getSourceAddress().empty());
    }

    return 0;
}
