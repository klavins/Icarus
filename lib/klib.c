/*
 * klib.c - Freestanding C library (memory, string, and printf routines)
 *
 * Copyright (C) 2026 Eric Klavins
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "klib.h"
#include "malloc.h"
#include <stdarg.h>

/* ---- Memory ---- */

void *memset(void *dest, int val, size_t len) {
    unsigned char *p = dest;
    /* Fill 8 bytes at a time using rep stosq */
    uint64_t v8 = (unsigned char)val;
    v8 |= v8 << 8;  v8 |= v8 << 16;  v8 |= v8 << 32;
    size_t qwords = len / 8;
    size_t tail = len % 8;
    if (qwords) {
        asm volatile ("rep stosq"
            : "+D"(p), "+c"(qwords)
            : "a"(v8)
            : "memory");
    }
    while (tail--)
        *p++ = (unsigned char)val;
    return dest;
}

void *memcpy(void *dest, const void *src, size_t len) {
    unsigned char *d = dest;
    const unsigned char *s = (const unsigned char *)src;
    /* Copy 8 bytes at a time using rep movsq */
    size_t qwords = len / 8;
    size_t tail = len % 8;
    if (qwords) {
        asm volatile ("rep movsq"
            : "+D"(d), "+S"(s), "+c"(qwords)
            :
            : "memory");
    }
    while (tail--)
        *d++ = *s++;
    return dest;
}

void *memmove(void *dest, const void *src, size_t len) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    if (d <= s || d >= s + len) {
        return memcpy(dest, src, len);
    }
    /* Overlap with dest after src — copy backwards */
    d += len;
    s += len;
    while (len--) *--d = *--s;
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

char *strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *d = malloc(len);
    if (d) memcpy(d, s, len);
    return d;
}

int strcasecmp(const char *a, const char *b) {
    while (*a && *b) {
        int ca = (*a >= 'a' && *a <= 'z') ? *a - 32 : *a;
        int cb = (*b >= 'a' && *b <= 'z') ? *b - 32 : *b;
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return *(unsigned char *)a - *(unsigned char *)b;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return (c == '\0') ? (char *)s : (void *)0;
}

int atoi(const char *s) {
    int neg = 0, val = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9')
        val = val * 10 + (*s++ - '0');
    return neg ? -val : val;
}

double atof(const char *s) {
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

char *strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack, *n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (char *)haystack;
    }
    return (void *)0;
}

/* ---- Character classification ---- */

int isdigit(int c) { return c >= '0' && c <= '9'; }
int isalpha(int c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
int isalnum(int c) { return isalpha(c) || isdigit(c); }
int isspace(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }
int isupper(int c) { return c >= 'A' && c <= 'Z'; }
int islower(int c) { return c >= 'a' && c <= 'z'; }
int isprint(int c) { return c >= 32 && c <= 126; }
int toupper(int c) { return islower(c) ? c - 32 : c; }
int tolower(int c) { return isupper(c) ? c + 32 : c; }

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

/* ---- sprintf / snprintf ---- */
/* Supports: %d %u %x %c %s %g %% */

/* Helper: emit one character, respecting the buffer limit */
static int emit(char *buf, size_t pos, size_t max, char c) {
    if (pos < max) buf[pos] = c;
    return 1;
}

/* Helper: emit a string */
static int emits(char *buf, size_t pos, size_t max, const char *s) {
    int n = 0;
    while (*s) {
        if (pos + n < max) buf[pos + n] = *s;
        s++;
        n++;
    }
    return n;
}

int vsnprintf(char *buf, size_t max, const char *fmt, va_list args) {
    size_t pos = 0;
    char tmp[33];

    while (*fmt) {
        if (*fmt != '%') {
            pos += emit(buf, pos, max, *fmt++);
            continue;
        }
        fmt++; /* skip '%' */

        switch (*fmt) {
        case 'd': {
            int val = va_arg(args, int);
            itoa(val, tmp, 10);
            pos += emits(buf, pos, max, tmp);
            break;
        }
        case 'u': {
            unsigned int val = va_arg(args, unsigned int);
            utoa(val, tmp, 10);
            pos += emits(buf, pos, max, tmp);
            break;
        }
        case 'x': {
            unsigned int val = va_arg(args, unsigned int);
            utoa(val, tmp, 16);
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

    /* Null-terminate: always, unless max == 0 */
    if (max > 0) buf[pos < max ? pos : max - 1] = '\0';
    return (int)pos;
}

int vsprintf(char *buf, const char *fmt, va_list args) {
    return vsnprintf(buf, (size_t)-1, fmt, args);
}

int snprintf(char *buf, size_t max, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buf, max, fmt, args);
    va_end(args);
    return ret;
}

int sprintf(char *buf, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buf, (size_t)-1, fmt, args);
    va_end(args);
    return ret;
}
