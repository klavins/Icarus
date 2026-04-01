/*
 * malloc.h - Dynamic memory allocation
 */
#ifndef ICARUS_USER_MALLOC_H
#define ICARUS_USER_MALLOC_H

#include "base.h"

#define malloc(s)               ((void *(*)(size_t))_icarus_api(API_MALLOC))(s)
#define realloc(p, s)           ((void *(*)(void*, size_t))_icarus_api(API_REALLOC))(p, s)
#define free(p)                 ((void (*)(void*))_icarus_api(API_FREE))(p)
#define calloc(n, s)            ((void *(*)(size_t, size_t))_icarus_api(API_CALLOC))(n, s)

#endif
