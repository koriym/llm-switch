#include <stdlib.h>
#include <string.h>

char *str_replace_all(const char *s, const char *from, const char *to) {
    size_t slen = strlen(s);
    size_t flen = strlen(from);
    if (flen == 0) {
        char *copy = malloc(slen + 1);
        if (copy) memcpy(copy, s, slen + 1);
        return copy;
    }

    size_t tlen = strlen(to);
    size_t count = 0;
    for (const char *p = s; (p = strstr(p, from)) != NULL; p += flen) count++;

    char *out = malloc(slen - count * flen + count * tlen + 1);
    if (out == NULL) return NULL;

    char *w = out;
    const char *p = s;
    const char *q;
    while ((q = strstr(p, from)) != NULL) {
        memcpy(w, p, (size_t)(q - p));
        w += q - p;
        memcpy(w, to, tlen);
        w += tlen;
        p = q + flen;
    }
    strcpy(w, p);
    return out;
}
