/*
 * string.h - String and memory functions
 */
#ifndef ICARUS_USER_STRING_H
#define ICARUS_USER_STRING_H

#include "base.h"

#define strlen(s)               ((size_t (*)(const char*))_icarus_api(API_STRLEN))(s)
#define strcmp(a, b)            ((int (*)(const char*, const char*))_icarus_api(API_STRCMP))(a, b)
#define strncmp(a, b, n)        ((int (*)(const char*, const char*, size_t))_icarus_api(API_STRNCMP))(a, b, n)
#define strcpy(d, s)            ((char *(*)(char*, const char*))_icarus_api(API_STRCPY))(d, s)
#define strncpy(d, s, n)        ((char *(*)(char*, const char*, size_t))_icarus_api(API_STRNCPY))(d, s, n)
#define strdup(s)               ((char *(*)(const char*))_icarus_api(API_STRDUP))(s)
#define strchr(s, c)            ((char *(*)(const char*, int))_icarus_api(API_STRCHR))(s, c)
#define strstr(h, n)            ((char *(*)(const char*, const char*))_icarus_api(API_STRSTR))(h, n)
#define memset(d, v, n)         ((void *(*)(void*, int, size_t))_icarus_api(API_MEMSET))(d, v, n)
#define memcpy(d, s, n)         ((void *(*)(void*, const void*, size_t))_icarus_api(API_MEMCPY))(d, s, n)
#define memmove(d, s, n)        ((void *(*)(void*, const void*, size_t))_icarus_api(API_MEMMOVE))(d, s, n)
#define memcmp(a, b, n)         ((int (*)(const void*, const void*, size_t))_icarus_api(API_MEMCMP))(a, b, n)

/* snprintf is variadic — get the function pointer, then call it normally */
#define snprintf                ((int (*)(char*, size_t, const char*, ...))_icarus_api(API_SNPRINTF))

#endif
