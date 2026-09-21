#include <stddef.h>
#include <string.h>

size_t utf8_truncate(char *s, size_t max_bytes) {
    size_t len = 0;
    while (s[len]) len++;
    if (len <= max_bytes) return len;

    size_t i = 0, end = 0;
    while (i < len) {
        unsigned char c = (unsigned char)s[i];
        size_t n = 1;
        if ((c & 0xE0) == 0xC0) n = 2;
        else if ((c & 0xF0) == 0xE0) n = 3;
        else if ((c & 0xF8) == 0xF0) n = 4;
        if (i + n > len) break;
        if (i + n > max_bytes) break;
        end = i + n;
        i += n;
    }
    s[end] = '\0';
    return end;
}
