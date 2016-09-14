#ifndef UTF8_H
#define UTF8_H

#include <cstddef>

namespace uWS {

bool isValidUtf8(unsigned char *str, size_t length);

}

#endif // UTF8_H
