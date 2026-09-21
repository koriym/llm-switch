#include <stddef.h>
#include <limits.h>

int parse_int_strict(const char *s, long *out) {
    if (s == NULL || out == NULL) return 0;

    const char *p = s;
    int neg = 0;
    if (*p == '+' || *p == '-') {
        neg = (*p == '-');
        p++;
    }
    if (*p < '0' || *p > '9') return 0;

    /* Accumulate the magnitude unsigned: -LONG_MIN does not fit in a long. */
    unsigned long limit = neg ? (unsigned long)LONG_MAX + 1UL : (unsigned long)LONG_MAX;
    unsigned long acc = 0;
    while (*p != '\0') {
        if (*p < '0' || *p > '9') return 0;
        unsigned long d = (unsigned long)(*p - '0');
        if (acc > (limit - d) / 10UL) return 0;
        acc = acc * 10UL + d;
        p++;
    }

    if (neg) *out = (acc == (unsigned long)LONG_MAX + 1UL) ? LONG_MIN : -(long)acc;
    else *out = (long)acc;
    return 1;
}
