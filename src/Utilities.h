#ifndef UTILITIES_H
#define UTILITIES_H

/* Various common utilities */

#include <cstdint>

namespace uWS {
namespace utils {

int u32toaHex(uint32_t value, char *dst) {
    char palette[] = "0123456789abcdef";
    char temp[10];
    char *p = temp;
    do {
        *p++ = palette[value % 16];
        value /= 16;
    } while (value > 0);

    int ret = p - temp;

    do {
        *dst++ = *--p;
    } while (p != temp);

    return ret;
}

int u32toa(uint32_t value, char *dst) {
    char temp[10];
    char *p = temp;
    do {
        *p++ = (char) (value % 10) + '0';
        value /= 10;
    } while (value > 0);

    int ret = p - temp;

    do {
        *dst++ = *--p;
    } while (p != temp);

    return ret;
}

}
}

#endif // UTILITIES_H
