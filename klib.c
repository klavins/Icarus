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

/* ---- dtoa ---- */
/* Simple double-to-string. Handles integers without decimal point,
   and floats with up to 6 significant decimal digits. */

static char *dtoa(double val, char *buf) {
    char *p = buf;

    /* Handle negative */
    if (val < 0) {
        *p++ = '-';
        val = -val;
    }

    /* Handle zero */
    if (val == 0.0) {
        *p++ = '0';
        *p = '\0';
        return buf;
    }

    /* If it's a whole number and fits in an int, use integer formatting */
    if (val == (double)(long long)(val) && val < 1e15 && val > -1e15) {
        long long iv = (long long)val;
        /* Write digits in reverse */
        char tmp[24];
        int i = 0;
        while (iv > 0) {
            tmp[i++] = '0' + (int)(iv % 10);
            iv /= 10;
        }
        if (i == 0) tmp[i++] = '0';
        while (i > 0)
            *p++ = tmp[--i];
        *p = '\0';
        return buf;
    }

    /* General float formatting */
    /* Normalize to get integer + fractional parts */
    int exp = 0;
    double norm = val;

    /* Scale to range [1, 10) */
    if (norm >= 10.0) {
        while (norm >= 10.0) { norm /= 10.0; exp++; }
    } else if (norm < 1.0) {
        while (norm < 1.0) { norm *= 10.0; exp--; }
    }

    /* If exponent is small, use fixed notation */
    if (exp >= -4 && exp <= 9) {
        /* Fixed notation */
        double frac;

        if (exp >= 0) {
            /* Shift decimal point right */
            double scale = 1.0;
            for (int i = 0; i < exp; i++) scale *= 10.0;
            long long ipart = (long long)(val);
            frac = val - (double)ipart;

            /* Integer part */
            char tmp[24];
            int ti = 0;
            if (ipart == 0) {
                tmp[ti++] = '0';
            } else {
                while (ipart > 0) {
                    tmp[ti++] = '0' + (int)(ipart % 10);
                    ipart /= 10;
                }
            }
            while (ti > 0)
                *p++ = tmp[--ti];
        } else {
            /* Number like 0.00123 */
            *p++ = '0';
            frac = val;
        }

        /* Fractional part — up to 6 digits, trim trailing zeros */
        if (frac > 0.0000005) {
            *p++ = '.';
            char fdigits[7];
            int fd = 0;
            for (int i = 0; i < 6; i++) {
                frac *= 10.0;
                int d = (int)frac;
                if (d > 9) d = 9;
                fdigits[fd++] = '0' + d;
                frac -= d;
            }
            /* Trim trailing zeros */
            while (fd > 0 && fdigits[fd-1] == '0') fd--;
            for (int i = 0; i < fd; i++)
                *p++ = fdigits[i];
        }
    } else {
        /* Scientific notation: d.dddddEsdd */
        int d = (int)norm;
        if (d > 9) d = 9;
        *p++ = '0' + d;
        double frac = norm - d;

        if (frac > 0.0000005) {
            *p++ = '.';
            char fdigits[7];
            int fd = 0;
            for (int i = 0; i < 6; i++) {
                frac *= 10.0;
                int di = (int)frac;
                if (di > 9) di = 9;
                fdigits[fd++] = '0' + di;
                frac -= di;
            }
            while (fd > 0 && fdigits[fd-1] == '0') fd--;
            for (int i = 0; i < fd; i++)
                *p++ = fdigits[i];
        }

        *p++ = 'E';
        if (exp < 0) {
            *p++ = '-';
            exp = -exp;
        } else {
            *p++ = '+';
        }
        if (exp >= 100) {
            *p++ = '0' + exp / 100;
            exp %= 100;
        }
        if (exp >= 10) {
            *p++ = '0' + exp / 10;
            exp %= 10;
        }
        *p++ = '0' + exp;
    }

    *p = '\0';
    return buf;
}

/* ---- sprintf ---- */
/* Supports: %d %u %x %c %s %g %% */

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
        case 'g': {
            double val = va_arg(args, double);
            dtoa(val, tmp);
            for (char *s = tmp; *s; )
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
