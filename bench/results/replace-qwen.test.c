#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

char *str_replace_all(const char *s, const char *from, const char *to);

static int check(const char *s, const char *from, const char *to, const char *expected) {
    char *result = str_replace_all(s, from, to);
    if (result == NULL) {
        if (expected == NULL) return 1;
        fprintf(stderr, "FAIL: str_replace_all(\"%s\", \"%s\", \"%s\") returned NULL, expected \"%s\"\n", s, from, to, expected);
        return 0;
    }
    if (expected == NULL) {
        fprintf(stderr, "FAIL: str_replace_all(\"%s\", \"%s\", \"%s\") returned \"%s\", expected NULL\n", s, from, to, result);
        free(result);
        return 0;
    }
    if (strcmp(result, expected) != 0) {
        fprintf(stderr, "FAIL: str_replace_all(\"%s\", \"%s\", \"%s\") returned \"%s\", expected \"%s\"\n", s, from, to, result, expected);
        free(result);
        return 0;
    }
    free(result);
    return 1;
}

int main(void) {
    int passed = 0;
    int total = 0;

    /* Test 1: basic replacement */
    total++;
    if (check("hello world", "world", "there", "hello there")) passed++;

    /* Test 2: multiple occurrences */
    total++;
    if (check("aaa", "a", "bb", "bbbbbb")) passed++;

    /* Test 3: no occurrence */
    total++;
    if (check("hello", "xyz", "abc", "hello")) passed++;

    /* Test 4: empty from string */
    total++;
    if (check("hello", "", "xyz", "hello")) passed++;

    /* Test 5: empty to string (deletion) */
    total++;
    if (check("hello", "l", "", "heo")) passed++;

    /* Test 6: empty source string */
    total++;
    if (check("", "a", "b", "")) passed++;

    /* Test 7: from longer than source */
    total++;
    if (check("ab", "abc", "x", "ab")) passed++;

    /* Test 8: replacement at start */
    total++;
    if (check("abcabc", "abc", "x", "xx")) passed++;

    /* Test 9: replacement at end */
    total++;
    if (check("xyzabc", "abc", "1", "xyz1")) passed++;

    /* Test 10: overlapping-like pattern (non-overlapping) */
    total++;
    if (check("aaaa", "aa", "b", "bb")) passed++;

    /* Test 11: from equals to */
    total++;
    if (check("test", "t", "t", "test")) passed++;

    /* Test 12: single char source, single char from/to */
    total++;
    if (check("a", "a", "b", "b")) passed++;

    /* Test 13: multiple different replacements */
    total++;
    if (check("aXbXc", "X", "12", "a12b12c")) passed++;

    /* Test 14: empty source with empty from */
    total++;
    if (check("", "", "x", "")) passed++;

    /* Test 15: from is substring of to */
    total++;
    if (check("ab", "a", "ab", "abb")) passed++;

    /* Test 16: to is substring of from */
    total++;
    if (check("abc", "abc", "a", "a")) passed++;

    /* Test 17: multiple occurrences with longer to */
    total++;
    if (check("123123", "123", "4567", "45674567")) passed++;

    /* Test 18: occurrence at very beginning */
    total++;
    if (check("abc", "abc", "z", "z")) passed++;

    /* Test 19: occurrence at very end */
    total++;
    if (check("xyz", "xyz", "z", "z")) passed++;

    /* Test 20: complex pattern */
    total++;
    if (check("foo bar foo baz foo", "foo", "qux", "qux bar qux baz qux")) passed++;

    printf("%d/%d tests passed\n", passed, total);
    return (passed == total) ? 0 : 1;
}
