#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

size_t lower_bound(const int *a, size_t n, int key);

static int tests_run = 0;
static int tests_failed = 0;

static void check(size_t n, const int *a, int key, size_t expected) {
    tests_run++;
    size_t result = lower_bound(a, n, key);
    if (result != expected) {
        tests_failed++;
        printf("FAIL: n=%zu, key=%d, expected=%zu, got=%zu\n", n, key, expected, result);
    }
}

int main(void) {
    /* Test 1: empty array */
    check(0, NULL, 5, 0);

    /* Test 2: single element, key less than element */
    int a1[] = {10};
    check(1, a1, 5, 0);

    /* Test 3: single element, key equal to element */
    check(1, a1, 10, 0);

    /* Test 4: single element, key greater than element */
    check(1, a1, 15, 1);

    /* Test 5: two elements, key less than both */
    int a2[] = {1, 3};
    check(2, a2, 0, 0);

    /* Test 6: two elements, key between */
    check(2, a2, 2, 1);

    /* Test 7: two elements, key equal to first */
    check(2, a2, 1, 0);

    /* Test 8: two elements, key equal to second */
    check(2, a2, 3, 1);

    /* Test 9: two elements, key greater than both */
    check(2, a2, 4, 2);

    /* Test 10: three elements, key less than all */
    int a3[] = {1, 2, 3};
    check(3, a3, 0, 0);

    /* Test 11: three elements, key equal to middle */
    check(3, a3, 2, 1);

    /* Test 12: three elements, key equal to last */
    check(3, a3, 3, 2);

    /* Test 13: three elements, key greater than all */
    check(3, a3, 4, 3);

    /* Test 14: duplicates, key equal to duplicate value */
    int a4[] = {1, 2, 2, 2, 3};
    check(5, a4, 2, 1);

    /* Test 15: duplicates, key less than duplicate value */
    check(5, a4, 1, 0);

    /* Test 16: duplicates, key greater than duplicate value */
    check(5, a4, 3, 4);

    /* Test 17: all same elements, key equal */
    int a5[] = {5, 5, 5, 5};
    check(4, a5, 5, 0);

    /* Test 18: all same elements, key less */
    check(4, a5, 4, 0);

    /* Test 19: all same elements, key greater */
    check(4, a5, 6, 4);

    /* Test 20: larger array, key in middle */
    int a6[] = {1, 3, 5, 7, 9, 11, 13, 15};
    check(8, a6, 7, 3);

    if (tests_failed == 0) {
        printf("All %d tests passed.\n", tests_run);
        return 0;
    } else {
        printf("%d of %d tests failed.\n", tests_failed, tests_run);
        return 1;
    }
}
