#include <stdio.h>
#include <assert.h>

/* Function under test */
static int icarus_atoi(const char *s) {
    int neg = 0, val = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9')
        val = val * 10 + (*s++ - '0');
    return neg ? -val : val;
}

int main(void) {
    /* 1. Basic positive */
    assert(icarus_atoi("42") == 42);
    printf("  1. basic positive = OK\n");

    /* 2. Basic negative */
    assert(icarus_atoi("-7") == -7);
    printf("  2. basic negative = OK\n");

    /* 3. Zero */
    assert(icarus_atoi("0") == 0);
    printf("  3. zero = OK\n");

    /* 4. Leading whitespace */
    assert(icarus_atoi("  123") == 123);
    assert(icarus_atoi("\t456") == 456);
    printf("  4. leading whitespace = OK\n");

    /* 5. Explicit positive sign */
    assert(icarus_atoi("+99") == 99);
    printf("  5. explicit + sign = OK\n");

    /* 6. Trailing non-digits ignored */
    assert(icarus_atoi("123abc") == 123);
    assert(icarus_atoi("42 ") == 42);
    printf("  6. trailing non-digits = OK\n");

    /* 7. Empty string */
    assert(icarus_atoi("") == 0);
    printf("  7. empty string = OK\n");

    /* 8. Just a sign */
    assert(icarus_atoi("-") == 0);
    assert(icarus_atoi("+") == 0);
    printf("  8. just a sign = OK\n");

    /* 9. Only whitespace */
    assert(icarus_atoi("   ") == 0);
    printf("  9. only whitespace = OK\n");

    /* 10. Large number */
    assert(icarus_atoi("2147483647") == 2147483647);
    printf(" 10. large positive = OK\n");

    /* 11. Large negative */
    assert(icarus_atoi("-2147483647") == -2147483647);
    printf(" 11. large negative = OK\n");

    /* 12. Leading zeros */
    assert(icarus_atoi("007") == 7);
    assert(icarus_atoi("000") == 0);
    printf(" 12. leading zeros = OK\n");

    /* 13. Negative zero */
    assert(icarus_atoi("-0") == 0);
    printf(" 13. negative zero = OK\n");

    /* 14. Non-numeric string */
    assert(icarus_atoi("abc") == 0);
    printf(" 14. non-numeric = OK\n");

    /* 15. Sign then non-digit */
    assert(icarus_atoi("-abc") == 0);
    printf(" 15. sign then non-digit = OK\n");

    /* 16. Whitespace then negative */
    assert(icarus_atoi("  -42") == -42);
    printf(" 16. whitespace then negative = OK\n");

    printf("\nAll tests passed.\n");
    return 0;
}
