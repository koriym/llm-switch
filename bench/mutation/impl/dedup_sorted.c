#include <stddef.h>

size_t dedup_sorted(int *a, size_t n) {
    if (n == 0) return 0;
    size_t w = 1;
    for (size_t i = 1; i < n; i++)
        if (a[i] != a[w - 1]) a[w++] = a[i];
    return w;
}
