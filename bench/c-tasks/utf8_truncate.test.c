#include <stddef.h>
#include <string.h>
#include <stdio.h>

size_t utf8_truncate(char *s, size_t max_bytes);

static int fails = 0;

#define CANARY 0x7f
#define BUFSZ 64

/* Strict: exact expected result for well-formed UTF-8 input. */
static void check(const char *name, const char *in, size_t max,
                  const char *want, size_t want_len) {
    char buf[BUFSZ];
    memset(buf, CANARY, sizeof buf);
    size_t n = strlen(in);
    memcpy(buf, in, n + 1);

    size_t got = utf8_truncate(buf, max);

    int ok = (got == want_len) && (strlen(buf) == want_len) &&
             (memcmp(buf, want, want_len) == 0) && (buf[want_len] == '\0');
    /* nothing beyond the original string may be touched */
    for (size_t i = n + 1; i < BUFSZ; i++)
        if ((unsigned char)buf[i] != CANARY) { ok = 0; break; }

    if (!ok) {
        fails++;
        printf("FAIL %-26s max=%2zu got=%zu want=%zu  [", name, max, got, want_len);
        for (size_t i = 0; i < got && i < 16; i++) printf("%02X ", (unsigned char)buf[i]);
        printf("] want [");
        for (size_t i = 0; i < want_len; i++) printf("%02X ", (unsigned char)want[i]);
        printf("]\n");
    } else {
        printf("ok   %-26s max=%2zu -> %zu\n", name, max, got);
    }
}

/* Safety only: invalid UTF-8 has no single correct answer.
   Require: result <= max, NUL-terminated at the returned length,
   no write past the original string, no absurd return value. */
static void check_safe(const char *name, const char *in, size_t max) {
    char buf[BUFSZ];
    memset(buf, CANARY, sizeof buf);
    size_t n = strlen(in);
    memcpy(buf, in, n + 1);

    size_t got = utf8_truncate(buf, max);

    int ok = (got <= max) && (got <= n) && (strlen(buf) == got);
    for (size_t i = n + 1; i < BUFSZ; i++)
        if ((unsigned char)buf[i] != CANARY) { ok = 0; break; }

    if (!ok) {
        fails++;
        printf("FAIL %-26s max=%2zu got=%zu (invalid: len<=max, NUL, no overrun)\n",
               name, max, got);
    } else {
        printf("ok   %-26s max=%2zu -> %zu (safe)\n", name, max, got);
    }
}

int main(void) {
    check("ascii fits",          "hello",              10, "hello", 5);
    check("ascii exact",         "hello",               5, "hello", 5);
    check("ascii cut",           "hello",               3, "hel",   3);
    check("max 0",               "hello",               0, "",      0);
    check("empty string",        "",                    5, "",      0);
    check("empty max 0",         "",                    0, "",      0);

    /* 2-byte: C3 A9 */
    check("2byte fits",          "\xC3\xA9",            2, "\xC3\xA9", 2);
    check("2byte split at 1",    "\xC3\xA9",            1, "",      0);
    check("2byte after ascii",   "a\xC3\xA9",           2, "a",     1);

    /* 3-byte: E2 82 AC */
    check("3byte fits",          "\xE2\x82\xAC",        3, "\xE2\x82\xAC", 3);
    check("3byte split at 2",    "\xE2\x82\xAC",        2, "",      0);
    check("3byte split at 1",    "\xE2\x82\xAC",        1, "",      0);

    /* 4-byte: F0 9F 98 80 */
    check("4byte fits",          "\xF0\x9F\x98\x80",    4, "\xF0\x9F\x98\x80", 4);
    check("4byte split at 3",    "\xF0\x9F\x98\x80",    3, "",      0);
    check("4byte split at 2",    "\xF0\x9F\x98\x80",    2, "",      0);
    check("4byte split at 1",    "\xF0\x9F\x98\x80",    1, "",      0);

    check("mixed cut mid 3byte", "ab\xE2\x82\xAC" "cd", 4, "ab",    2);
    check("mixed keep 3byte",    "ab\xE2\x82\xAC" "cd", 5, "ab\xE2\x82\xAC", 5);

    check_safe("stray continuation", "\x82\x83\x84",    2);
    check_safe("lone lead byte",     "\xF0",            1);
    /* already fits: must be returned unchanged even if the tail is malformed */
    check("malformed tail fits",  "a\xE2\x82",          10, "a\xE2\x82", 3);
    check_safe("truncated seq",      "a\xE2\x82",       3);

    printf(fails ? "\n%d FAILED\n" : "\nALL PASSED\n", fails);
    return fails != 0;
}
