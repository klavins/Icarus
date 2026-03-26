#include "klib.h"
#include <stdarg.h>

/* ---- Memory ---- */

void *memset(void *dest, int val, size_t len) {
    unsigned char *p = dest;
    while (len--)
        *p++ = (unsigned char)val;
    return dest;
}

void *memcpy(void *dest, const void *src, size_t len) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (len--)
        *d++ = *s++;
    return dest;
}

int memcmp(const void *a, const void *b, size_t len) {
    const unsigned char *pa = a, *pb = b;
    while (len--) {
        if (*pa != *pb)
            return *pa - *pb;
        pa++;
        pb++;
    }
    return 0;
}

/* ---- Strings ---- */

size_t strlen(const char *s) {
    size_t n = 0;
    while (*s++)
        n++;
    return n;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *(unsigned char *)a - *(unsigned char *)b;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    return n ? *(unsigned char *)a - *(unsigned char *)b : 0;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++))
        ;
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    char *d = dest;
    while (n && (*d++ = *src++))
        n--;
    while (n--)
        *d++ = '\0';
    return dest;
}

/* ---- Number to string ---- */

char *utoa(unsigned int value, char *buf, int base) {
    static const char digits[] = "0123456789abcdef";
    char tmp[33];
    int i = 0;

    if (base < 2 || base > 16) {
        buf[0] = '\0';
        return buf;
    }

    do {
        tmp[i++] = digits[value % base];
        value /= base;
    } while (value);

    int j = 0;
    while (i--)
        buf[j++] = tmp[i];
    buf[j] = '\0';
    return buf;
}

char *itoa(int value, char *buf, int base) {
    if (base == 10 && value < 0) {
        buf[0] = '-';
        utoa((unsigned int)(-(long)value), buf + 1, base);
        return buf;
    }
    return utoa((unsigned int)value, buf, base);
}

/* ---- sprintf ---- */
/* Supports: %d %u %x %c %s %% */

int vsprintf(char *buf, const char *fmt, va_list args) {
    char *out = buf;
    char tmp[33];

    while (*fmt) {
        if (*fmt != '%') {
            *out++ = *fmt++;
            continue;
        }
        fmt++; /* skip '%' */

        switch (*fmt) {
        case 'd': {
            int val = va_arg(args, int);
            itoa(val, tmp, 10);
            for (char *s = tmp; *s; )
                *out++ = *s++;
            break;
        }
        case 'u': {
            unsigned int val = va_arg(args, unsigned int);
            utoa(val, tmp, 10);
            for (char *s = tmp; *s; )
                *out++ = *s++;
            break;
        }
        case 'x': {
            unsigned int val = va_arg(args, unsigned int);
            utoa(val, tmp, 16);
            for (char *s = tmp; *s; )
                *out++ = *s++;
            break;
        }
        case 'c': {
            char c = (char)va_arg(args, int);
            *out++ = c;
            break;
        }
        case 's': {
            const char *s = va_arg(args, const char *);
            if (!s) s = "(null)";
            while (*s)
                *out++ = *s++;
            break;
        }
        case '%':
            *out++ = '%';
            break;
        default:
            *out++ = '%';
            *out++ = *fmt;
            break;
        }
        fmt++;
    }

    *out = '\0';
    return (int)(out - buf);
}

int sprintf(char *buf, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vsprintf(buf, fmt, args);
    va_end(args);
    return ret;
}
