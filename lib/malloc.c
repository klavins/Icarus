/*
 * malloc.c - Dynamic memory allocator (first-fit free list)
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

#include "malloc.h"
#include "klib.h"

#define ALIGN 8
#define BLOCK_MAGIC 0xA110CA7E

/*
 * Block list is maintained in address order — split inserts new blocks
 * between the current block and its successor, preserving this invariant.
 * coalesce relies on address-ordered neighbors to merge correctly.
 */
struct block {
    size_t size;       /* usable size (excludes header) */
    int free;
    uint32_t magic;
    struct block *next;
};

#define HEADER_SIZE ((sizeof(struct block) + (ALIGN - 1)) & ~(ALIGN - 1))

static struct block *free_list;

void heap_init(void *base, size_t size) {
    /* Align base up to ALIGN boundary */
    uintptr_t addr = ((uintptr_t)base + (ALIGN - 1)) & ~(ALIGN - 1);
    size -= (addr - (uintptr_t)base);

    free_list = (struct block *)addr;
    free_list->size = size - HEADER_SIZE;
    free_list->free = 1;
    free_list->magic = BLOCK_MAGIC;
    free_list->next = (void *)0;
}

/* Split a block if it's large enough to hold the request plus a new block */
static void split(struct block *b, size_t size) {
    if (b->size < size + HEADER_SIZE + ALIGN) return; /* not worth splitting */
    size_t remaining = b->size - size - HEADER_SIZE;

    struct block *new = (struct block *)((char *)b + HEADER_SIZE + size);
    new->size = remaining;
    new->free = 1;
    new->magic = BLOCK_MAGIC;
    new->next = b->next;

    b->size = size;
    b->next = new;
}

void *malloc(size_t size) {
    if (size == 0) return (void *)0;

    /* Align request */
    size = (size + (ALIGN - 1)) & ~(ALIGN - 1);

    /* First-fit search */
    struct block *b = free_list;
    while (b) {
        if (b->free && b->size >= size) {
            split(b, size);
            b->free = 0;
            return (char *)b + HEADER_SIZE;
        }
        b = b->next;
    }
    return (void *)0; /* out of memory */
}

/* Merge a free block with its immediate neighbors */
static void coalesce(struct block *b) {
    /* Merge with next block if free */
    while (b->next && b->next->free) {
        b->size += HEADER_SIZE + b->next->size;
        b->next = b->next->next;
    }
    /* Merge with previous block if free */
    struct block *prev = free_list;
    while (prev && prev->next != b)
        prev = prev->next;
    if (prev && prev->free) {
        prev->size += HEADER_SIZE + b->size;
        prev->next = b->next;
    }
}

void free(void *ptr) {
    if (!ptr) return;
    struct block *b = (struct block *)((char *)ptr - HEADER_SIZE);
    if (b->magic != BLOCK_MAGIC) return; /* bad pointer */
    b->free = 1;
    coalesce(b);
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) { free(ptr); return (void *)0; }

    struct block *b = (struct block *)((char *)ptr - HEADER_SIZE);
    if (b->magic != BLOCK_MAGIC) return (void *)0;

    /* Align for fair comparison with b->size */
    size_t aligned = (size + (ALIGN - 1)) & ~(ALIGN - 1);

    if (b->size >= aligned) return ptr; /* already big enough */

    /* Try to grow in place by absorbing the next block */
    if (b->next && b->next->free &&
        b->size + HEADER_SIZE + b->next->size >= aligned) {
        b->size += HEADER_SIZE + b->next->size;
        b->next = b->next->next;
        split(b, aligned);
        return ptr;
    }

    void *new = malloc(size);
    if (!new) return (void *)0;
    memcpy(new, ptr, b->size);
    free(ptr);
    return new;
}

void *calloc(size_t count, size_t size) {
    if (count && size > (size_t)-1 / count) return (void *)0; /* overflow */
    size_t total = count * size;
    void *p = malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

size_t heap_free_total(void) {
    size_t total = 0;
    struct block *b = free_list;
    while (b) {
        if (b->free) total += b->size;
        b = b->next;
    }
    return total;
}
