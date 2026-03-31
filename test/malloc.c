#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stddef.h>

/* Pull in malloc.c directly, but skip klib.h — macOS provides memcpy/memset */
#define KLIB_H
#include "../lib/malloc.c"

static char heap[1024 * 1024]; /* 1 MB fake heap */

int main(void) {
    heap_init(heap, sizeof(heap));

    /* 1. Basic allocation returns non-NULL */
    void *a = malloc(100);
    assert(a != NULL);
    printf("  1. malloc(100) = OK\n");

    /* 2. Can write and read back allocated memory */
    memset(a, 0xAA, 100);
    for (int i = 0; i < 100; i++)
        assert(((unsigned char *)a)[i] == 0xAA);
    printf("  2. write + readback = OK\n");

    /* 3. Multiple allocations don't overlap */
    void *b = malloc(200);
    assert(b != NULL);
    memset(b, 0xBB, 200);
    /* Verify a's contents weren't corrupted by writing to b */
    for (int i = 0; i < 100; i++)
        assert(((unsigned char *)a)[i] == 0xAA);
    for (int i = 0; i < 200; i++)
        assert(((unsigned char *)b)[i] == 0xBB);
    printf("  3. no overlap (cross-verified) = OK\n");

    /* 4. Free and reuse: first-fit returns the same block */
    free(a);
    void *c = malloc(100);
    assert(c != NULL);
    assert(c == a); /* first-fit reuses the freed block */
    free(c);
    free(b);
    printf("  4. free + first-fit reuse = OK\n");

    /* 5. Realloc preserves data */
    void *d = malloc(50);
    memset(d, 0x55, 50);
    void *e = realloc(d, 500);
    assert(e != NULL);
    for (int i = 0; i < 50; i++)
        assert(((unsigned char *)e)[i] == 0x55);
    free(e);
    printf("  5. realloc preserves data = OK\n");

    /* 6. Calloc zeroes previously-dirty memory */
    void *dirty = malloc(100);
    memset(dirty, 0xFF, 100);
    free(dirty);
    void *f = calloc(10, 10);
    assert(f != NULL);
    assert(f == dirty); /* confirm we got the same block back */
    for (int i = 0; i < 100; i++)
        assert(((unsigned char *)f)[i] == 0);
    free(f);
    printf("  6. calloc zeroed (same dirty block) = OK\n");

    /* 7. Heap free tracks both alloc and free */
    size_t before = heap_free_total();
    void *g = malloc(1000);
    size_t during = heap_free_total();
    assert(during < before);
    free(g);
    size_t after = heap_free_total();
    assert(after > during);
    printf("  7. heap_free_total alloc+free = OK\n");

    /* 8. malloc(0) returns NULL */
    assert(malloc(0) == NULL);
    printf("  8. malloc(0) = NULL = OK\n");

    /* 9. free(NULL) doesn't crash */
    free(NULL);
    printf("  9. free(NULL) = OK\n");

    /* 10. realloc(NULL, n) acts as malloc */
    void *h = realloc(NULL, 64);
    assert(h != NULL);
    free(h);
    printf(" 10. realloc(NULL, n) = OK\n");

    /* 11. realloc(ptr, 0) frees and returns NULL (our implementation) */
    void *i = malloc(64);
    void *j = realloc(i, 0);
    assert(j == NULL);
    printf(" 11. realloc(ptr, 0) = OK\n");

    /* 12. Many small allocations and frees — reverse + interleaved */
    void *ptrs[100];
    for (int k = 0; k < 100; k++)
        ptrs[k] = malloc(32);
    for (int k = 0; k < 100; k++)
        assert(ptrs[k] != NULL);
    /* Free in reverse order */
    for (int k = 99; k >= 0; k--)
        free(ptrs[k]);
    /* Allocate again and free interleaved (evens, then odds) */
    for (int k = 0; k < 100; k++)
        ptrs[k] = malloc(32);
    for (int k = 0; k < 100; k += 2)
        free(ptrs[k]);
    for (int k = 1; k < 100; k += 2)
        free(ptrs[k]);
    /* Verify heap is still usable after all that */
    void *post = malloc(2048);
    assert(post != NULL);
    memset(post, 0xCC, 2048);
    free(post);
    printf(" 12. 100 alloc/free (rev+interleaved) + post-alloc = OK\n");

    /* 13. Coalescing in non-sequential free order */
    heap_init(heap, sizeof(heap));
    void *x1 = malloc(1000);
    void *x2 = malloc(1000);
    void *x3 = malloc(1000);
    /* Free outer blocks first, then middle — forces bidirectional merge */
    free(x1);
    free(x3);
    free(x2); /* must merge with both x1 (prev) and x3 (next) */
    size_t big = heap_free_total() - 64;
    void *m = malloc(big);
    assert(m != NULL);
    free(m);
    printf(" 13. coalesce (non-sequential free) = OK\n");

    /* 14. Realloc grows in place when next block is free */
    heap_init(heap, sizeof(heap));
    void *p = malloc(64);
    void *q = malloc(64);
    void *r = malloc(64);
    memset(p, 0x11, 64);
    free(q); /* free the block right after p */
    void *p2 = realloc(p, 120);
    assert(p2 == p); /* should grow in place, same pointer */
    for (int k = 0; k < 64; k++)
        assert(((unsigned char *)p2)[k] == 0x11); /* data preserved */
    free(p2);
    free(r);
    printf(" 14. realloc grows in place = OK\n");

    /* 15. calloc overflow returns NULL */
    void *ov = calloc((size_t)-1, 2);
    assert(ov == NULL);
    printf(" 15. calloc overflow = OK\n");

    /* 16. Unaligned heap_init still works */
    static char heap2[4096];
    heap_init(heap2 + 1, sizeof(heap2) - 1); /* deliberately misaligned */
    void *ua = malloc(32);
    assert(ua != NULL);
    assert(((uintptr_t)ua % 8) == 0); /* returned pointer is aligned */
    free(ua);
    printf(" 16. unaligned heap_init = OK\n");

    /* 17. Double free: second free is benign (block already free) */
    heap_init(heap, sizeof(heap));
    void *df = malloc(64);
    free(df);
    size_t free_before = heap_free_total();
    free(df); /* block already free — coalesce should be idempotent */
    size_t free_after = heap_free_total();
    assert(free_after == free_before); /* heap didn't grow or corrupt */
    void *df2 = malloc(64);
    assert(df2 != NULL);
    free(df2);
    printf(" 17. double free benign = OK\n");

    /* 18. Realloc to smaller size returns same pointer */
    heap_init(heap, sizeof(heap));
    void *rs = malloc(200);
    memset(rs, 0x77, 200);
    void *rs2 = realloc(rs, 50);
    assert(rs2 == rs); /* should return same pointer */
    for (int k = 0; k < 50; k++)
        assert(((unsigned char *)rs2)[k] == 0x77);
    free(rs2);
    printf(" 18. realloc shrink = OK\n");

    printf("\nAll tests passed.\n");
    return 0;
}
