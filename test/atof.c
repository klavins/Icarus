#include <stdio.h>
#include <assert.h>
#include <math.h>

/* Function under test */
static double icarus_atof(const char *s) {
    double val = 0, frac = 0, div = 1;
    int neg = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9')
        val = val * 10 + (*s++ - '0');
    if (*s == '.') {
        s++;
        while (*s >= '0' && *s <= '9') {
            frac = frac * 10 + (*s++ - '0');
            div *= 10;
        }
        val += frac / div;
    }
    return neg ? -val : val;
}

#define CLOSE(a, b) (fabs((a) - (b)) < 1e-9)

int main(void) {
    /* 1. Basic integer */
    assert(CLOSE(icarus_atof("42"), 42.0));
    printf("  1. basic integer = OK\n");

    /* 2. Basic decimal */
    assert(CLOSE(icarus_atof("3.14"), 3.14));
    printf("  2. basic decimal = OK\n");

    /* 3. Negative */
    assert(CLOSE(icarus_atof("-2.5"), -2.5));
    printf("  3. negative = OK\n");

    /* 4. Zero */
    assert(CLOSE(icarus_atof("0"), 0.0));
    assert(CLOSE(icarus_atof("0.0"), 0.0));
    printf("  4. zero = OK\n");

    /* 5. Leading whitespace */
    assert(CLOSE(icarus_atof("  1.5"), 1.5));
    assert(CLOSE(icarus_atof("\t2.5"), 2.5));
    printf("  5. leading whitespace = OK\n");

    /* 6. Explicit positive sign */
    assert(CLOSE(icarus_atof("+3.0"), 3.0));
    printf("  6. explicit + sign = OK\n");

    /* 7. No integer part */
    assert(CLOSE(icarus_atof(".5"), 0.5));
    printf("  7. no integer part = OK\n");

    /* 8. No fractional part */
    assert(CLOSE(icarus_atof("7."), 7.0));
    printf("  8. no fractional part = OK\n");

    /* 9. Trailing non-digits */
    assert(CLOSE(icarus_atof("1.5abc"), 1.5));
    assert(CLOSE(icarus_atof("99 "), 99.0));
    printf("  9. trailing non-digits = OK\n");

    /* 10. Empty string */
    assert(CLOSE(icarus_atof(""), 0.0));
    printf(" 10. empty string = OK\n");

    /* 11. Just a sign */
    assert(CLOSE(icarus_atof("-"), 0.0));
    assert(CLOSE(icarus_atof("+"), 0.0));
    printf(" 11. just a sign = OK\n");

    /* 12. Only whitespace */
    assert(CLOSE(icarus_atof("   "), 0.0));
    printf(" 12. only whitespace = OK\n");

    /* 13. Many decimal places */
    assert(CLOSE(icarus_atof("1.123456789"), 1.123456789));
    printf(" 13. many decimal places = OK\n");

    /* 14. Large number */
    assert(CLOSE(icarus_atof("1000000.5"), 1000000.5));
    printf(" 14. large number = OK\n");

    /* 15. Negative zero */
    assert(CLOSE(icarus_atof("-0.0"), 0.0));
    printf(" 15. negative zero = OK\n");

    /* 16. Leading zeros */
    assert(CLOSE(icarus_atof("007.007"), 7.007));
    printf(" 16. leading zeros = OK\n");

    /* 17. Non-numeric string */
    assert(CLOSE(icarus_atof("abc"), 0.0));
    printf(" 17. non-numeric = OK\n");

    /* 18. Small fractional */
    assert(CLOSE(icarus_atof("0.001"), 0.001));
    printf(" 18. small fractional = OK\n");

    printf("\nAll tests passed.\n");
    return 0;
}
