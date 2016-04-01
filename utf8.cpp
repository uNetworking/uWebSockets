/*
 * The utf8_check() function scans the '\0'-terminated string starting
 * at s. It returns a pointer to the first byte of the first malformed
 * or overlong UTF-8 sequence found, or NULL if the string contains
 * only correct UTF-8. It also spots UTF-8 sequences that could cause
 * trouble if converted to UTF-16, namely surrogate characters
 * (U+D800..U+DFFF) and non-Unicode positions (U+FFFE..U+FFFF). This
 * routine is very likely to find a malformed sequence if the input
 * uses any other encoding than UTF-8. It therefore can be used as a
 * very effective heuristic for distinguishing between UTF-8 and other
 * encodings.
 *
 * I wrote this code mainly as a specification of functionality; there
 * are no doubt performance optimizations possible for certain CPUs.
 *
 * Markus Kuhn <http://www.cl.cam.ac.uk/~mgk25/> -- 2005-03-30
 * License: http://www.cl.cam.ac.uk/~mgk25/short-license.html
 */

// Modified to accept (U+FFFE..U+FFFF), according to Autobahn testing

namespace uWS {

unsigned char *utf8_check(unsigned char *s, unsigned char *e)
{
  while (s != e) {
    if (*s < 0x80)
      /* 0xxxxxxx */
      s++;
    else if ((s[0] & 0xe0) == 0xc0) {
      /* 110XXXXx 10xxxxxx */
      if ((s[1] & 0xc0) != 0x80 ||
      (s[0] & 0xfe) == 0xc0)                        /* overlong? */
    return s;
      else
    s += 2;
    } else if ((s[0] & 0xf0) == 0xe0) {
      /* 1110XXXX 10Xxxxxx 10xxxxxx */
      if ((s[1] & 0xc0) != 0x80 ||
      (s[2] & 0xc0) != 0x80 ||
      (s[0] == 0xe0 && (s[1] & 0xe0) == 0x80) ||    /* overlong? */
      (s[0] == 0xed && (s[1] & 0xe0) == 0xa0) /*||*/    /* surrogate? */
      /*(s[0] == 0xef && s[1] == 0xbf &&
       (s[2] & 0xfe) == 0xbe)*/)                      /* U+FFFE or U+FFFF? */
    return s;
      else
    s += 3;
    } else if ((s[0] & 0xf8) == 0xf0) {
      /* 11110XXX 10XXxxxx 10xxxxxx 10xxxxxx */
      if ((s[1] & 0xc0) != 0x80 ||
      (s[2] & 0xc0) != 0x80 ||
      (s[3] & 0xc0) != 0x80 ||
      (s[0] == 0xf0 && (s[1] & 0xf0) == 0x80) ||    /* overlong? */
      (s[0] == 0xf4 && s[1] > 0x8f) || s[0] > 0xf4) /* > U+10FFFF? */
    return s;
      else
    s += 4;
    } else
      return s;
  }

  return nullptr;
}

}
