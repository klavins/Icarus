#ifndef KLIB_H
#define KLIB_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

/* Memory */
void *memset(void *dest, int val, size_t len);
void *memcpy(void *dest, const void *src, size_t len);
int   memcmp(const void *a, const void *b, size_t len);

/* Strings */
size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strcpy(char *dest, const char *src);
char  *strncpy(char *dest, const char *src, size_t n);

/* Number to string */
char *itoa(int value, char *buf, int base);
char *utoa(unsigned int value, char *buf, int base);

/* Formatted printing into a buffer */
int vsprintf(char *buf, const char *fmt, va_list args);
int sprintf(char *buf, const char *fmt, ...);

#endif
