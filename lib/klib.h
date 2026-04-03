/*
 * klib.h - Freestanding C library function declarations
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

#ifndef KLIB_H
#define KLIB_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

/* Memory */
void *memset(void *dest, int val, size_t len);
void *memcpy(void *dest, const void *src, size_t len);
void *memmove(void *dest, const void *src, size_t len);
int   memcmp(const void *a, const void *b, size_t len);

/* Strings */
size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strcpy(char *dest, const char *src);
char  *strncpy(char *dest, const char *src, size_t n);
char  *strdup(const char *s);
int    strcasecmp(const char *a, const char *b);
char  *strchr(const char *s, int c);
char  *strstr(const char *haystack, const char *needle);

/* Character classification */
int isdigit(int c);
int isalpha(int c);
int isalnum(int c);
int isspace(int c);
int isupper(int c);
int islower(int c);
int isprint(int c);
int toupper(int c);
int tolower(int c);

/* Number to string */
char *itoa(int value, char *buf, int base);
char *utoa(unsigned int value, char *buf, int base);

/* Formatted printing into a buffer */
int vsnprintf(char *buf, size_t max, const char *fmt, va_list args);
int vsprintf(char *buf, const char *fmt, va_list args);
int snprintf(char *buf, size_t max, const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);

#endif
