#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>

int parse_int_strict(const char *s, long *out);

static int check(const char *s, int expected_ret, long expected_val) {
    long out = 0;
    int ret = parse_int_strict(s, &out);
    if (ret != expected_ret) return 0;
    if (expected_ret == 1 && out != expected_val) return 0;
    return 1;
}

int main(void) {
    int fails = 0;
    long out;

    /* 1. NULL input */
    if (!check(NULL, 0, 0)) fails++;

    /* 2. NULL output */
    if (parse_int_strict("123", NULL) != 0) fails++;

    /* 3. Empty string */
    if (!check("", 0, 0)) fails++;

    /* 4. Simple positive */
    if (!check("123", 1, 123)) fails++;

    /* 5. Simple negative */
    if (!check("-123", 1, -123)) fails++;

    /* 6. Plus sign */
    if (!check("+123", 1, 123)) fails++;

    /* 7. Zero */
    if (!check("0", 1, 0)) fails++;

    /* 8. Negative zero */
    if (!check("-0", 1, 0)) fails++;

    /* 9. Plus zero */
    if (!check("+0", 1, 0)) fails++;

    /* 10. LONG_MAX */
    if (!check("9223372036854775807", 1, LONG_MAX)) fails++;

    /* 11. LONG_MIN */
    if (!check("-9223372036854775808", 1, LONG_MIN)) fails++;

    /* 12. Overflow positive */
    if (!check("9223372036854775808", 0, 0)) fails++;

    /* 13. Overflow negative */
    if (!check("-9223372036854775809", 0, 0)) fails++;

    /* 14. Invalid char after digits */
    if (!check("123a", 0, 0)) fails++;

    /* 15. Invalid char before digits */
    if (!check("a123", 0, 0)) fails++;

    /* 16. Sign only */
    if (!check("+", 0, 0)) fails++;

    /* 17. Sign only negative */
    if (!check("-", 0, 0)) fails++;

    /* 18. Leading zeros */
    if (!check("000123", 1, 123)) fails++;

    /* 19. Large negative within range */
    if (!check("-1000000000000000000", 1, -1000000000000000000L)) fails++;

    /* 20. Large positive within range */
    if (!check("1000000000000000000", 1, 1000000000000000000L)) fails++;

    if (fails) {
        printf("FAIL: %d checks failed\n", fails);
        return 1;
    }
    printf("PASS\n");
    return 0;
}
