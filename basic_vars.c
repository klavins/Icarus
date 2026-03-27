#include "basic_internal.h"

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
        terminal_print("?TOO MANY VARIABLES\n");
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
    char val[MAX_STR_LEN];
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
    if (i >= 0) return str_vars[i].val;
    return "";
}

void strvar_set(const char *name, const char *val) {
    int i = strvar_find(name);
    if (i < 0) {
        terminal_print("?STRING NOT DIMMED\n");
        return;
    }
    if (!str_vars[i].dimmed) {
        terminal_print("?STRING NOT DIMMED\n");
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
            terminal_print("?TOO MANY VARIABLES\n");
            return;
        }
        i = str_var_count++;
        strncpy(str_vars[i].name, name, MAX_VAR_NAME - 1);
        str_vars[i].name[MAX_VAR_NAME - 1] = '\0';
    }
    if (size < 1) size = 1;
    if (size > MAX_STR_LEN) size = MAX_STR_LEN;
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
    double vals[MAX_ARRAY_SIZE];
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
            terminal_print("?TOO MANY VARIABLES\n");
            return;
        }
        i = array_count++;
        strncpy(arrays[i].name, name, MAX_VAR_NAME - 1);
        arrays[i].name[MAX_VAR_NAME - 1] = '\0';
    }
    if (s1 < 1) s1 = 1;
    if (s2 < 0) s2 = 0;
    int total = s2 > 0 ? s1 * s2 : s1;
    if (total > MAX_ARRAY_SIZE) {
        terminal_print("?ARRAY TOO LARGE\n");
        return;
    }
    arrays[i].dimmed = 1;
    arrays[i].size1 = s1;
    arrays[i].size2 = s2;
    memset(arrays[i].vals, 0, total * sizeof(double));
}

double array_get(const char *name, int i1, int i2) {
    int a = array_find(name);
    if (a < 0 || !arrays[a].dimmed) {
        terminal_print("?ARRAY NOT DIMMED\n");
        return 0.0;
    }
    int idx;
    if (arrays[a].size2 > 0) {
        if (i1 < 0 || i1 >= arrays[a].size1 || i2 < 0 || i2 >= arrays[a].size2) {
            terminal_print("?BAD SUBSCRIPT\n");
            return 0.0;
        }
        idx = i1 * arrays[a].size2 + i2;
    } else {
        if (i1 < 0 || i1 >= arrays[a].size1) {
            terminal_print("?BAD SUBSCRIPT\n");
            return 0.0;
        }
        idx = i1;
    }
    return arrays[a].vals[idx];
}

void array_set(const char *name, int i1, int i2, double val) {
    int a = array_find(name);
    if (a < 0 || !arrays[a].dimmed) {
        terminal_print("?ARRAY NOT DIMMED\n");
        return;
    }
    int idx;
    if (arrays[a].size2 > 0) {
        if (i1 < 0 || i1 >= arrays[a].size1 || i2 < 0 || i2 >= arrays[a].size2) {
            terminal_print("?BAD SUBSCRIPT\n");
            return;
        }
        idx = i1 * arrays[a].size2 + i2;
    } else {
        if (i1 < 0 || i1 >= arrays[a].size1) {
            terminal_print("?BAD SUBSCRIPT\n");
            return;
        }
        idx = i1;
    }
    arrays[a].vals[idx] = val;
}
