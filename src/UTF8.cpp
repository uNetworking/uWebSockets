#include "UTF8.h"

#include <cstdint>

namespace uWS {

// Based on utf8_check.c by Markus Kuhn, 2005
// https://www.cl.cam.ac.uk/~mgk25/ucs/utf8_check.c
// Optimized for predominantly 7-bit content, 2016
bool isValidUtf8(unsigned char *s, size_t length)
{
    for (unsigned char *e = s + length; s != e; ) {
        if (s + 4 <= e && ((*(uint32_t *) s) & 0x80808080) == 0) {
            s += 4;
        } else {
            while (!(*s & 0x80)) {
                if (++s == e) {
                    return true;
                }
            }

            if ((s[0] & 0x60) == 0x40) {
                if ((s[1] & 0xc0) != 0x80 || (s[0] & 0xfe) == 0xc0) {
                    return false;
                }
                s += 2;
            } else if ((s[0] & 0xf0) == 0xe0) {
                if ((s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80 ||
                        (s[0] == 0xe0 && (s[1] & 0xe0) == 0x80) || (s[0] == 0xed && (s[1] & 0xe0) == 0xa0)) {
                    return false;
                }
                s += 3;
            } else if ((s[0] & 0xf8) == 0xf0) {
                if ((s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80 || (s[3] & 0xc0) != 0x80 ||
                        (s[0] == 0xf0 && (s[1] & 0xf0) == 0x80) || (s[0] == 0xf4 && s[1] > 0x8f) || s[0] > 0xf4) {
                    return false;
                }
                s += 4;
            } else {
                return false;
            }
        }
    }
    return true;
}

}
