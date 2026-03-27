#include "basic_internal.h"
#include "klib.h"

/* ---- Bump allocator ---- */

static uint8_t *heap_base;
static size_t heap_size;
static size_t heap_pos;
static size_t heap_watermark;

void basic_heap_init(void) {
    /* Read heap info from boot info at 0x80000 */
    struct {
        uint32_t addr_lo, width, height, pitch, bpp, total_memory_kb;
        char firmware_vendor[64];
        uint32_t firmware_revision, pixel_format;
        uint32_t heap_base, heap_size;
    } *info = (void *)0x80000;

    if (info->heap_base && info->heap_size) {
        heap_base = (uint8_t *)(uintptr_t)info->heap_base;
        heap_size = info->heap_size;
    } else {
        /* Fallback: use memory at 16MB for non-UEFI builds */
        heap_base = (uint8_t *)0x1000000;
        heap_size = 4 * 1024 * 1024;
    }
    heap_pos = 0;
    heap_watermark = 0;
}

void *basic_alloc(size_t size) {
    /* Align to 8 bytes */
    size = (size + 7) & ~(size_t)7;
    if (heap_pos + size > heap_size) return (void *)0;
    void *p = heap_base + heap_pos;
    heap_pos += size;
    return p;
}

void basic_alloc_set_watermark(void) {
    heap_watermark = heap_pos;
}

void basic_alloc_reset(void) {
    heap_pos = heap_watermark;
}

size_t basic_heap_free(void) {
    return heap_size - heap_pos;
}

/* ---- Numeric variables ---- */

static struct {
    char name[MAX_VAR_NAME];
    double val;
} num_vars[MAX_VARS];
int num_var_count;

int var_find(const char *name) {
    for (int i = 0; i < num_var_count; i++)
        if (strcmp(num_vars[i].name, name) == 0)
            return i;
    return -1;
}

double var_get(const char *name) {
    int i = var_find(name);
    if (i >= 0) return num_vars[i].val;
    return 0.0;
}

void var_set(const char *name, double val) {
    int i = var_find(name);
    if (i >= 0) {
        num_vars[i].val = val;
        return;
    }
    if (num_var_count >= MAX_VARS) {
        os_print("?TOO MANY VARIABLES\n");
        return;
    }
    strncpy(num_vars[num_var_count].name, name, MAX_VAR_NAME - 1);
    num_vars[num_var_count].name[MAX_VAR_NAME - 1] = '\0';
    num_vars[num_var_count].val = val;
    num_var_count++;
}

/* ---- String variables ---- */

static struct {
    char name[MAX_VAR_NAME];
    int  dimmed;
    int  maxlen;
    char *val;
} str_vars[MAX_VARS];
int str_var_count;

int strvar_find(const char *name) {
    for (int i = 0; i < str_var_count; i++)
        if (strcmp(str_vars[i].name, name) == 0)
            return i;
    return -1;
}

const char *strvar_get(const char *name) {
    int i = strvar_find(name);
    if (i >= 0 && str_vars[i].val) return str_vars[i].val;
    return "";
}

void strvar_set(const char *name, const char *val) {
    int i = strvar_find(name);
    if (i < 0) {
        os_print("?STRING NOT DIMMED\n");
        return;
    }
    if (!str_vars[i].dimmed || !str_vars[i].val) {
        os_print("?STRING NOT DIMMED\n");
        return;
    }
    int j;
    for (j = 0; val[j] && j < str_vars[i].maxlen - 1; j++)
        str_vars[i].val[j] = val[j];
    str_vars[i].val[j] = '\0';
}

void strvar_dim(const char *name, int size) {
    int i = strvar_find(name);
    if (i < 0) {
        if (str_var_count >= MAX_VARS) {
            os_print("?TOO MANY VARIABLES\n");
            return;
        }
        i = str_var_count++;
        strncpy(str_vars[i].name, name, MAX_VAR_NAME - 1);
        str_vars[i].name[MAX_VAR_NAME - 1] = '\0';
    }
    if (size < 1) size = 1;
    if (size > MAX_STR_LEN) size = MAX_STR_LEN;
    str_vars[i].val = basic_alloc(size);
    if (!str_vars[i].val) {
        os_print("?OUT OF MEMORY\n");
        return;
    }
    str_vars[i].dimmed = 1;
    str_vars[i].maxlen = size;
    str_vars[i].val[0] = '\0';
}

/* ---- Arrays ---- */

static struct {
    char name[MAX_VAR_NAME];
    int  dimmed;
    int  size1;
    int  size2;
    double *vals;
} arrays[MAX_VARS];
int array_count;

int array_find(const char *name) {
    for (int i = 0; i < array_count; i++)
        if (strcmp(arrays[i].name, name) == 0)
            return i;
    return -1;
}

void array_dim(const char *name, int s1, int s2) {
    int i = array_find(name);
    if (i < 0) {
        if (array_count >= MAX_VARS) {
            os_print("?TOO MANY VARIABLES\n");
            return;
        }
        i = array_count++;
        strncpy(arrays[i].name, name, MAX_VAR_NAME - 1);
        arrays[i].name[MAX_VAR_NAME - 1] = '\0';
    }
    if (s1 < 1) s1 = 1;
    if (s2 < 0) s2 = 0;
    int total = s2 > 0 ? s1 * s2 : s1;
    arrays[i].vals = basic_alloc(total * sizeof(double));
    if (!arrays[i].vals) {
        os_print("?OUT OF MEMORY\n");
        return;
    }
    arrays[i].dimmed = 1;
    arrays[i].size1 = s1;
    arrays[i].size2 = s2;
    memset(arrays[i].vals, 0, total * sizeof(double));
}

double array_get(const char *name, int i1, int i2) {
    int a = array_find(name);
    if (a < 0 || !arrays[a].dimmed || !arrays[a].vals) {
        os_print("?ARRAY NOT DIMMED\n");
        return 0.0;
    }
    int idx;
    if (arrays[a].size2 > 0) {
        if (i1 < 0 || i1 >= arrays[a].size1 || i2 < 0 || i2 >= arrays[a].size2) {
            os_print("?BAD SUBSCRIPT\n");
            return 0.0;
        }
        idx = i1 * arrays[a].size2 + i2;
    } else {
        if (i1 < 0 || i1 >= arrays[a].size1) {
            os_print("?BAD SUBSCRIPT\n");
            return 0.0;
        }
        idx = i1;
    }
    return arrays[a].vals[idx];
}

void array_set(const char *name, int i1, int i2, double val) {
    int a = array_find(name);
    if (a < 0 || !arrays[a].dimmed || !arrays[a].vals) {
        os_print("?ARRAY NOT DIMMED\n");
        return;
    }
    int idx;
    if (arrays[a].size2 > 0) {
        if (i1 < 0 || i1 >= arrays[a].size1 || i2 < 0 || i2 >= arrays[a].size2) {
            os_print("?BAD SUBSCRIPT\n");
            return;
        }
        idx = i1 * arrays[a].size2 + i2;
    } else {
        if (i1 < 0 || i1 >= arrays[a].size1) {
            os_print("?BAD SUBSCRIPT\n");
            return;
        }
        idx = i1;
    }
    arrays[a].vals[idx] = val;
}
