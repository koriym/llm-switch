#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>

size_t dedup_sorted(int *a, size_t n);

static int tests_run = 0;
static int tests_failed = 0;

static void check(const char *name, int *a, size_t n, size_t expected_len, const int *expected_vals) {
    tests_run++;
    size_t got_len = dedup_sorted(a, n);
    int ok = (got_len == expected_len);
    if (ok && expected_len > 0) {
        if (memcmp(a, expected_vals, expected_len * sizeof(int)) != 0) {
            ok = 0;
        }
    }
    if (!ok) {
        tests_failed++;
        fprintf(stderr, "FAIL: %s\n", name);
        fprintf(stderr, "  expected len=%zu, got len=%zu\n", expected_len, got_len);
        fprintf(stderr, "  expected: [");
        for (size_t i = 0; i < expected_len; i++) {
            fprintf(stderr, "%d%s", expected_vals[i], i + 1 < expected_len ? ", " : "");
        }
        fprintf(stderr, "]\n");
        fprintf(stderr, "  got:      [");
        for (size_t i = 0; i < got_len; i++) {
            fprintf(stderr, "%d%s", a[i], i + 1 < got_len ? ", " : "");
        }
        fprintf(stderr, "]\n");
    }
}

int main(void) {
    /* Test 1: empty array */
    {
        int a[1] = {0};
        check("empty", a, 0, 0, NULL);
    }

    /* Test 2: single element */
    {
        int a[1] = {42};
        int exp[1] = {42};
        check("single", a, 1, 1, exp);
    }

    /* Test 3: two distinct */
    {
        int a[2] = {1, 2};
        int exp[2] = {1, 2};
        check("two_distinct", a, 2, 2, exp);
    }

    /* Test 4: two same */
    {
        int a[2] = {5, 5};
        int exp[1] = {5};
        check("two_same", a, 2, 1, exp);
    }

    /* Test 5: all same */
    {
        int a[5] = {7, 7, 7, 7, 7};
        int exp[1] = {7};
        check("all_same", a, 5, 1, exp);
    }

    /* Test 6: all distinct */
    {
        int a[5] = {1, 2, 3, 4, 5};
        int exp[5] = {1, 2, 3, 4, 5};
        check("all_distinct", a, 5, 5, exp);
    }

    /* Test 7: duplicates at start */
    {
        int a[5] = {1, 1, 2, 3, 4};
        int exp[4] = {1, 2, 3, 4};
        check("dup_start", a, 5, 4, exp);
    }

    /* Test 8: duplicates at end */
    {
        int a[5] = {1, 2, 3, 4, 4};
        int exp[4] = {1, 2, 3, 4};
        check("dup_end", a, 5, 4, exp);
    }

    /* Test 9: duplicates in middle */
    {
        int a[5] = {1, 2, 2, 3, 4};
        int exp[4] = {1, 2, 3, 4};
        check("dup_mid", a, 5, 4, exp);
    }

    /* Test 10: multiple groups */
    {
        int a[8] = {1, 1, 2, 2, 2, 3, 3, 4};
        int exp[4] = {1, 2, 3, 4};
        check("multi_groups", a, 8, 4, exp);
    }

    /* Test 11: negative numbers */
    {
        int a[5] = {-3, -3, -1, 0, 0};
        int exp[3] = {-3, -1, 0};
        check("negatives", a, 5, 3, exp);
    }

    /* Test 12: INT_MIN and INT_MAX */
    {
        int a[4] = {INT_MIN, INT_MIN, INT_MAX, INT_MAX};
        int exp[2] = {INT_MIN, INT_MAX};
        check("extremes", a, 4, 2, exp);
    }

    /* Test 13: alternating pairs */
    {
        int a[6] = {1, 1, 2, 2, 3, 3};
        int exp[3] = {1, 2, 3};
        check("alt_pairs", a, 6, 3, exp);
    }

    /* Test 14: large array with many duplicates */
    {
        int a[10];
        int exp[5];
        for (int i = 0; i < 10; i++) a[i] = i / 2;
        for (int i = 0; i < 5; i++) exp[i] = i;
        check("large_dup", a, 10, 5, exp);
    }

    /* Test 15: consecutive duplicates of varying lengths */
    {
        int a[10] = {1, 1, 1, 2, 2, 3, 4, 4, 4, 4};
        int exp[4] = {1, 2, 3, 4};
        check("varying_dup", a, 10, 4, exp);
    }

    /* Test 16: two elements, second different */
    {
        int a[2] = {100, 200};
        int exp[2] = {100, 200};
        check("two_diff", a, 2, 2, exp);
    }

    /* Test 17: three elements, first two same */
    {
        int a[3] = {5, 5, 6};
        int exp[2] = {5, 6};
        check("three_first_two", a, 3, 2, exp);
    }

    /* Test 18: three elements, last two same */
    {
        int a[3] = {5, 6, 6};
        int exp[2] = {5, 6};
        check("three_last_two", a, 3, 2, exp);
    }

    /* Test 19: three elements, all same */
    {
        int a[3] = {9, 9, 9};
        int exp[1] = {9};
        check("three_all_same", a, 3, 1, exp);
    }

    /* Test 20: three elements, all distinct */
    {
        int a[3] = {1, 2, 3};
        int exp[3] = {1, 2, 3};
        check("three_all_distinct", a, 3, 3, exp);
    }

    if (tests_failed > 0) {
        fprintf(stderr, "%d/%d tests failed\n", tests_failed, tests_run);
        return 1;
    }
    return 0;
}
