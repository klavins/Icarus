/*
 * Tests for klib functions. Uses icarus_ prefixed copies of the exact
 * source from lib/klib.c. The x86 asm fast paths (rep movsq/stosq) are
 * replaced with portable C — the logic under test is the same.
 *
 * If you change a function in lib/klib.c, update the copy here too.
 * This is a trade-off forced by macOS's fortified string.h macros which
 * prevent defining functions named memset/memcpy/etc.
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

/* --- Stub malloc for strdup --- */
static char strdup_pool[4096];
static size_t strdup_pos;
static void *icarus_malloc(size_t size) {
    void *p = strdup_pool + strdup_pos;
    strdup_pos += (size + 7) & ~(size_t)7;
    return p;
}

/* --- Functions under test (from lib/klib.c, portable versions) --- */

static void *icarus_memset(void *dest, int val, size_t len) {
    unsigned char *p = dest;
    while (len--) *p++ = (unsigned char)val;
    return dest;
}

static void *icarus_memcpy(void *dest, const void *src, size_t len) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (len--) *d++ = *s++;
    return dest;
}

static void *icarus_memmove(void *dest, const void *src, size_t len) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    if (d <= s || d >= s + len) {
        return icarus_memcpy(dest, src, len);
    }
    d += len;
    s += len;
    while (len--) *--d = *--s;
    return dest;
}

static int icarus_memcmp(const void *a, const void *b, size_t len) {
    const unsigned char *pa = a, *pb = b;
    while (len--) {
        if (*pa != *pb) return *pa - *pb;
        pa++; pb++;
    }
    return 0;
}

static size_t icarus_strlen(const char *s) {
    size_t n = 0;
    while (*s++) n++;
    return n;
}

static int icarus_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *(unsigned char *)a - *(unsigned char *)b;
}

static char *icarus_strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

static char *icarus_strdup(const char *s) {
    size_t len = icarus_strlen(s) + 1;
    char *d = icarus_malloc(len);
    if (d) icarus_memcpy(d, s, len);
    return d;
}

int main(void) {
    /* ---- memset ---- */

    char buf[64];

    /* 1. Fill with non-zero and verify every byte */
    icarus_memset(buf, 0xBB, 32);
    for (int i = 0; i < 32; i++)
        assert((unsigned char)buf[i] == 0xBB);
    printf("  1. memset non-zero fill = OK\n");

    /* 2. Fill with zero */
    buf[0] = 0xFF;
    icarus_memset(buf, 0, 32);
    for (int i = 0; i < 32; i++)
        assert(buf[i] == 0);
    printf("  2. memset zero fill = OK\n");

    /* ---- memcpy ---- */

    /* 3. Copy and verify contents byte-by-byte */
    char ref[64];
    for (int i = 0; i < 37; i++) ref[i] = (char)(i + 1);
    icarus_memset(buf, 0, sizeof(buf));
    icarus_memcpy(buf, ref, 37);
    for (int i = 0; i < 37; i++)
        assert(buf[i] == (char)(i + 1));
    /* Verify bytes after copy region are untouched */
    assert(buf[37] == 0);
    printf("  3. memcpy contents verified = OK\n");

    /* ---- memmove ---- */

    /* 4. Non-overlapping */
    icarus_memcpy(buf, "ABCDEFGH", 8);
    icarus_memset(buf + 32, 0, 8);
    icarus_memmove(buf + 32, buf, 8);
    assert(icarus_memcmp(buf + 32, "ABCDEFGH", 8) == 0);
    printf("  4. memmove non-overlapping = OK\n");

    /* 5. Overlap: dest after src (backward copy needed) */
    icarus_memcpy(buf, "ABCDEFGHIJ", 10);
    icarus_memmove(buf + 2, buf, 8);
    assert(icarus_memcmp(buf + 2, "ABCDEFGH", 8) == 0);
    assert(buf[0] == 'A' && buf[1] == 'B'); /* bytes before dest untouched */
    printf("  5. memmove overlap dest>src = OK\n");

    /* 6. Overlap: dest before src (forward copy) */
    icarus_memcpy(buf, "xxABCDEFGH", 10);
    icarus_memmove(buf, buf + 2, 8);
    assert(icarus_memcmp(buf, "ABCDEFGH", 8) == 0);
    assert(buf[9] == 'H'); /* byte after moved region untouched */
    printf("  6. memmove overlap dest<src = OK\n");

    /* 7. Same pointer */
    icarus_memcpy(buf, "HELLO", 5);
    icarus_memmove(buf, buf, 5);
    assert(icarus_memcmp(buf, "HELLO", 5) == 0);
    printf("  7. memmove same pointer = OK\n");

    /* 8. Zero length doesn't modify anything */
    icarus_memmove(buf, buf + 10, 0);
    assert(icarus_memcmp(buf, "HELLO", 5) == 0);
    printf("  8. memmove zero length = OK\n");

    /* 9. Overlap by 1 byte */
    icarus_memcpy(buf, "ABCDEF", 6);
    icarus_memmove(buf + 1, buf, 5);
    assert(icarus_memcmp(buf + 1, "ABCDE", 5) == 0);
    printf("  9. memmove overlap by 1 = OK\n");

    /* ---- memcmp ---- */

    /* 10. Equal regions */
    assert(icarus_memcmp("ABCD", "ABCD", 4) == 0);
    printf(" 10. memcmp equal = OK\n");

    /* 11. First less than second */
    assert(icarus_memcmp("ABCD", "ABCE", 4) < 0);
    printf(" 11. memcmp less = OK\n");

    /* 12. First greater than second */
    assert(icarus_memcmp("ABCE", "ABCD", 4) > 0);
    printf(" 12. memcmp greater = OK\n");

    /* 13. Zero length always equal */
    assert(icarus_memcmp("X", "Y", 0) == 0);
    printf(" 13. memcmp zero length = OK\n");

    /* ---- strlen ---- */

    /* 14. Normal string */
    assert(icarus_strlen("hello") == 5);
    printf(" 14. strlen normal = OK\n");

    /* 15. Empty string */
    assert(icarus_strlen("") == 0);
    printf(" 15. strlen empty = OK\n");

    /* 16. Single char */
    assert(icarus_strlen("x") == 1);
    printf(" 16. strlen single = OK\n");

    /* ---- strcmp ---- */

    /* 17. Equal */
    assert(icarus_strcmp("abc", "abc") == 0);
    printf(" 17. strcmp equal = OK\n");

    /* 18. Less */
    assert(icarus_strcmp("abc", "abd") < 0);
    printf(" 18. strcmp less = OK\n");

    /* 19. Greater */
    assert(icarus_strcmp("abd", "abc") > 0);
    printf(" 19. strcmp greater = OK\n");

    /* 20. Prefix — shorter string is less */
    assert(icarus_strcmp("ab", "abc") < 0);
    printf(" 20. strcmp prefix = OK\n");

    /* ---- strcpy ---- */

    /* 21. Copy and verify byte-by-byte */
    icarus_memset(buf, 0xFF, sizeof(buf));
    icarus_strcpy(buf, "hello");
    assert(buf[0] == 'h' && buf[1] == 'e' && buf[2] == 'l');
    assert(buf[3] == 'l' && buf[4] == 'o');
    assert(buf[5] == '\0'); /* null terminator written */
    assert((unsigned char)buf[6] == 0xFF); /* didn't overwrite past null */
    printf(" 21. strcpy = OK\n");

    /* ---- strdup ---- */

    /* 22. Basic */
    char *s = icarus_strdup("hello");
    assert(s != NULL);
    assert(icarus_strcmp(s, "hello") == 0);
    printf(" 22. strdup basic = OK\n");

    /* 23. Independent copy */
    char orig[] = "world";
    char *s2 = icarus_strdup(orig);
    orig[0] = 'X';
    assert(icarus_strcmp(s2, "world") == 0);
    printf(" 23. strdup independent = OK\n");

    /* 24. Empty string */
    char *s3 = icarus_strdup("");
    assert(s3 != NULL);
    assert(s3[0] == '\0');
    printf(" 24. strdup empty = OK\n");

    printf("\nAll tests passed.\n");
    return 0;
}
