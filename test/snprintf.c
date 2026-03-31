/*
 * Tests for icarus snprintf/vsnprintf. Duplicates the implementation
 * from klib.c to avoid x86 asm / macOS fortified header conflicts.
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>

/* --- Supporting functions (from klib.c) --- */

static char *icarus_utoa(unsigned int value, char *buf, int base) {
    static const char digits[] = "0123456789abcdef";
    char tmp[33];
    int i = 0;
    if (base < 2 || base > 16) { buf[0] = '\0'; return buf; }
    do { tmp[i++] = digits[value % base]; value /= base; } while (value);
    int j = 0;
    while (i--) buf[j++] = tmp[i];
    buf[j] = '\0';
    return buf;
}

static char *icarus_itoa(int value, char *buf, int base) {
    if (base == 10 && value < 0) {
        buf[0] = '-';
        icarus_utoa((unsigned int)(-(long)value), buf + 1, base);
        return buf;
    }
    return icarus_utoa((unsigned int)value, buf, base);
}

static char *dtoa(double val, char *buf) {
    if (val < 0) { *buf = '-'; icarus_itoa((int)(-val), buf + 1, 10); }
    else icarus_itoa((int)val, buf, 10);
    return buf;
}

/* --- snprintf implementation (from klib.c) --- */

static int emit(char *buf, size_t pos, size_t max, char c) {
    if (pos < max) buf[pos] = c;
    return 1;
}

static int emits(char *buf, size_t pos, size_t max, const char *s) {
    int n = 0;
    while (*s) {
        if (pos + n < max) buf[pos + n] = *s;
        s++;
        n++;
    }
    return n;
}

static int icarus_vsnprintf(char *buf, size_t max, const char *fmt, va_list args) {
    size_t pos = 0;
    char tmp[33];

    while (*fmt) {
        if (*fmt != '%') {
            pos += emit(buf, pos, max, *fmt++);
            continue;
        }
        fmt++;

        switch (*fmt) {
        case 'd': {
            int val = va_arg(args, int);
            icarus_itoa(val, tmp, 10);
            pos += emits(buf, pos, max, tmp);
            break;
        }
        case 'u': {
            unsigned int val = va_arg(args, unsigned int);
            icarus_utoa(val, tmp, 10);
            pos += emits(buf, pos, max, tmp);
            break;
        }
        case 'x': {
            unsigned int val = va_arg(args, unsigned int);
            icarus_utoa(val, tmp, 16);
            pos += emits(buf, pos, max, tmp);
            break;
        }
        case 'c': {
            char c = (char)va_arg(args, int);
            pos += emit(buf, pos, max, c);
            break;
        }
        case 's': {
            const char *s = va_arg(args, const char *);
            if (!s) s = "(null)";
            pos += emits(buf, pos, max, s);
            break;
        }
        case 'g': {
            double val = va_arg(args, double);
            dtoa(val, tmp);
            pos += emits(buf, pos, max, tmp);
            break;
        }
        case '%':
            pos += emit(buf, pos, max, '%');
            break;
        default:
            pos += emit(buf, pos, max, '%');
            pos += emit(buf, pos, max, *fmt);
            break;
        }
        fmt++;
    }

    if (max > 0) buf[pos < max ? pos : max - 1] = '\0';
    return (int)pos;
}

static int icarus_snprintf(char *buf, size_t max, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = icarus_vsnprintf(buf, max, fmt, args);
    va_end(args);
    return ret;
}

static int icarus_sprintf(char *buf, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = icarus_vsnprintf(buf, (size_t)-1, fmt, args);
    va_end(args);
    return ret;
}

/* --- Tests --- */

int main(void) {
    char buf[256];
    int ret;

    /* 1. Basic string */
    ret = icarus_snprintf(buf, sizeof(buf), "hello %s", "world");
    assert(strcmp(buf, "hello world") == 0);
    assert(ret == 11);
    printf("  1. snprintf basic string = OK\n");

    /* 2. Integer */
    ret = icarus_snprintf(buf, sizeof(buf), "%d", 42);
    assert(strcmp(buf, "42") == 0);
    assert(ret == 2);
    printf("  2. snprintf integer = OK\n");

    /* 3. Negative integer */
    ret = icarus_snprintf(buf, sizeof(buf), "%d", -7);
    assert(strcmp(buf, "-7") == 0);
    assert(ret == 2);
    printf("  3. snprintf negative = OK\n");

    /* 4. Hex */
    ret = icarus_snprintf(buf, sizeof(buf), "%x", 0xFF);
    assert(strcmp(buf, "ff") == 0);
    assert(ret == 2);
    printf("  4. snprintf hex = OK\n");

    /* 5. Unsigned */
    ret = icarus_snprintf(buf, sizeof(buf), "%u", 4000000000u);
    assert(strcmp(buf, "4000000000") == 0);
    assert(ret == 10);
    printf("  5. snprintf unsigned = OK\n");

    /* 6. Char */
    ret = icarus_snprintf(buf, sizeof(buf), "%c", 'Z');
    assert(buf[0] == 'Z');
    assert(buf[1] == '\0');
    assert(ret == 1);
    printf("  6. snprintf char = OK\n");

    /* 7. Percent literal */
    ret = icarus_snprintf(buf, sizeof(buf), "100%%");
    assert(strcmp(buf, "100%") == 0);
    assert(ret == 4);
    printf("  7. snprintf percent = OK\n");

    /* 8. NULL string */
    ret = icarus_snprintf(buf, sizeof(buf), "%s", (char *)0);
    assert(strcmp(buf, "(null)") == 0);
    assert(ret == 6);
    printf("  8. snprintf NULL string = OK\n");

    /* 9. Multiple format specifiers */
    ret = icarus_snprintf(buf, sizeof(buf), "%s=%d (0x%x)", "val", 255, 255);
    assert(strcmp(buf, "val=255 (0xff)") == 0);
    assert(ret == 14);
    printf("  9. snprintf multiple specs = OK\n");

    /* 10. Truncation: buffer smaller than output */
    memset(buf, 'X', sizeof(buf));
    ret = icarus_snprintf(buf, 6, "hello world");
    assert(buf[5] == '\0');                  /* null-terminated */
    assert(memcmp(buf, "hello", 5) == 0);    /* truncated content */
    assert(ret == 11);                        /* return value = full length */
    printf(" 10. snprintf truncation = OK\n");

    /* 11. Truncation mid-format-specifier */
    ret = icarus_snprintf(buf, 4, "%d", 123456);
    assert(buf[3] == '\0');                   /* null-terminated */
    assert(memcmp(buf, "123", 3) == 0);       /* partial number */
    assert(ret == 6);                          /* full length would be 6 */
    printf(" 11. snprintf truncate number = OK\n");

    /* 12. Buffer size 1: just null terminator */
    ret = icarus_snprintf(buf, 1, "hello");
    assert(buf[0] == '\0');
    assert(ret == 5);
    printf(" 12. snprintf size 1 = OK\n");

    /* 13. Buffer size 0: doesn't write anything */
    buf[0] = 'X';
    ret = icarus_snprintf(buf, 0, "hello");
    assert(buf[0] == 'X');  /* untouched */
    assert(ret == 5);
    printf(" 13. snprintf size 0 = OK\n");

    /* 14. Empty format string */
    ret = icarus_snprintf(buf, sizeof(buf), "");
    assert(buf[0] == '\0');
    assert(ret == 0);
    printf(" 14. snprintf empty format = OK\n");

    /* 15. sprintf (unbounded) works */
    ret = icarus_sprintf(buf, "x=%d y=%d", 10, 20);
    assert(strcmp(buf, "x=10 y=20") == 0);
    assert(ret == 9);
    printf(" 15. sprintf unbounded = OK\n");

    /* 16. Truncation preserves sentinel byte after buffer */
    memset(buf, 'S', sizeof(buf));
    ret = icarus_snprintf(buf, 8, "abcdefghijklmnop");
    assert(buf[7] == '\0');              /* null-terminated at limit */
    assert(memcmp(buf, "abcdefg", 7) == 0);
    assert(buf[8] == 'S');               /* sentinel untouched */
    assert(ret == 16);
    printf(" 16. snprintf no overrun = OK\n");

    printf("\nAll tests passed.\n");
    return 0;
}
