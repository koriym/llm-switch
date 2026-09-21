#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>

size_t utf8_truncate(char *s, size_t max_bytes);

static int check(const char *input, size_t max_bytes, size_t expected_ret, const char *expected_str) {
    char buf[256];
    size_t input_len = strlen(input);
    if (input_len >= sizeof(buf)) {
        fprintf(stderr, "Input too long\n");
        return 1;
    }
    strcpy(buf, input);
    
    size_t ret = utf8_truncate(buf, max_bytes);
    
    if (ret != expected_ret) {
        fprintf(stderr, "FAIL: input=\"%s\" max_bytes=%zu expected_ret=%zu got_ret=%zu\n", input, max_bytes, expected_ret, ret);
        return 1;
    }
    
    if (strcmp(buf, expected_str) != 0) {
        fprintf(stderr, "FAIL: input=\"%s\" max_bytes=%zu expected_str=\"%s\" got_str=\"%s\"\n", input, max_bytes, expected_str, buf);
        return 1;
    }
    
    return 0;
}

int main(void) {
    int failures = 0;
    
    /* Test 1: Empty string */
    failures += check("", 0, 0, "");
    
    /* Test 2: ASCII string shorter than max_bytes */
    failures += check("abc", 10, 3, "abc");
    
    /* Test 3: ASCII string exactly max_bytes */
    failures += check("abc", 3, 3, "abc");
    
    /* Test 4: ASCII string longer than max_bytes */
    failures += check("abcdef", 3, 3, "abc");
    
    /* Test 5: Two-byte UTF-8 char, max_bytes cuts in middle */
    /* "é" is 0xC3 0xA9 (2 bytes) */
    failures += check("\xC3\xA9", 1, 0, "");
    
    /* Test 6: Two-byte UTF-8 char, max_bytes fits exactly */
    failures += check("\xC3\xA9", 2, 2, "\xC3\xA9");
    
    /* Test 7: Two-byte UTF-8 char followed by ASCII, max_bytes cuts before second char */
    /* "éa" is 0xC3 0xA9 0x61 (3 bytes) */
    failures += check("\xC3\xA9a", 2, 2, "\xC3\xA9");
    
    /* Test 8: Two-byte UTF-8 char followed by ASCII, max_bytes cuts in middle of first char */
    failures += check("\xC3\xA9a", 1, 0, "");
    
    /* Test 9: Three-byte UTF-8 char, max_bytes cuts in middle */
    /* "€" is 0xE2 0x82 0xAC (3 bytes) */
    failures += check("\xE2\x82\xAC", 1, 0, "");
    
    /* Test 10: Three-byte UTF-8 char, max_bytes cuts after first byte */
    failures += check("\xE2\x82\xAC", 2, 0, "");
    
    /* Test 11: Three-byte UTF-8 char, max_bytes fits exactly */
    failures += check("\xE2\x82\xAC", 3, 3, "\xE2\x82\xAC");
    
    /* Test 12: Four-byte UTF-8 char, max_bytes cuts in middle */
    /* "😀" is 0xF0 0x9F 0x98 0x80 (4 bytes) */
    failures += check("\xF0\x9F\x98\x80", 1, 0, "");
    
    /* Test 13: Four-byte UTF-8 char, max_bytes cuts after 2 bytes */
    failures += check("\xF0\x9F\x98\x80", 2, 0, "");
    
    /* Test 14: Four-byte UTF-8 char, max_bytes cuts after 3 bytes */
    failures += check("\xF0\x9F\x98\x80", 3, 0, "");
    
    /* Test 15: Four-byte UTF-8 char, max_bytes fits exactly */
    failures += check("\xF0\x9F\x98\x80", 4, 4, "\xF0\x9F\x98\x80");
    
    /* Test 16: Mixed: two-byte + three-byte + four-byte, max_bytes cuts between chars */
    /* "é€😀" = 2+3+4 = 9 bytes */
    /* max_bytes=5 should keep "é€" (5 bytes) */
    failures += check("\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80", 5, 5, "\xC3\xA9\xE2\x82\xAC");
    
    /* Test 17: Mixed, max_bytes cuts in middle of three-byte char */
    /* max_bytes=3 should keep "é" (2 bytes) */
    failures += check("\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80", 3, 2, "\xC3\xA9");
    
    /* Test 18: Mixed, max_bytes cuts in middle of four-byte char */
    /* max_bytes=8 should keep "é€" (5 bytes) */
    failures += check("\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80", 8, 5, "\xC3\xA9\xE2\x82\xAC");
    
    /* Test 19: max_bytes=0 with non-empty string */
    failures += check("abc", 0, 0, "");
    
    /* Test 20: max_bytes=0 with empty string */
    failures += check("", 0, 0, "");
    
    if (failures == 0) {
        printf("All tests passed\n");
        return 0;
    } else {
        printf("%d test(s) failed\n", failures);
        return 1;
    }
}
