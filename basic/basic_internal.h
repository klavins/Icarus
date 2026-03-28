#ifndef BASIC_INTERNAL_H
#define BASIC_INTERNAL_H

#include "basic.h"
#include "os.h"
#include "graphics.h"

/* ---- Constants ---- */

#define MAX_VARS       128
#define MAX_VAR_NAME   32
#define MAX_STR_LEN    256
#define MAX_LINES      10000
#define MAX_LINE_LEN   256
#define MAX_FOR_DEPTH  32
#define MAX_GOSUB_DEPTH 64
#define MAX_DATA       4096

/* ---- Program store (defined in basic.c) ---- */

struct program_line {
    int linenum;
    char *text;
};

extern struct program_line program[];
extern int program_count;

int program_find(int linenum);

/* ---- Shared state: expression evaluator (defined in basic_expr.c) ---- */

extern struct token *tok_pos;
extern int expr_overflow;

/* ---- Shared state: executor (defined in basic_exec.c) ---- */

extern int running;
extern int next_line_idx;
extern int current_linenum;  /* line number of currently executing line, 0 if immediate */

struct for_entry {
    char var[MAX_VAR_NAME];
    double limit;
    double step;
    int program_idx;
};

extern struct for_entry for_stack[];
extern int for_depth;

extern int gosub_stack[];
extern int gosub_depth;

struct data_item {
    int is_string;
    double number_val;
    char string_val[MAX_TOKEN_LEN];
};

extern struct data_item data_store[];
extern int data_count;
extern int data_ptr;

/* ---- Shared state: variables (defined in basic_vars.c) ---- */

extern int num_var_count;
extern int str_var_count;
extern int array_count;

/* ---- Variable functions (basic_vars.c) ---- */

int         var_find(const char *name);
double      var_get(const char *name);
void        var_set(const char *name, double val);
int         strvar_find(const char *name);
const char *strvar_get(const char *name);
void        strvar_set(const char *name, const char *val);
void        strvar_dim(const char *name, int size);
int         array_find(const char *name);
void        array_dim(const char *name, int s1, int s2);
double      array_get(const char *name, int i1, int i2);
void        array_set(const char *name, int i1, int i2, double val);

/* ---- Tokenizer helpers (basic_token.c) ---- */

int  is_alpha(char c);
int  is_digit(char c);
int  is_alnum(char c);
char to_upper(char c);

/* ---- Expression evaluator (basic_expr.c) ---- */

double parse_expr(void);
double parse_condition(void);

/* ---- Executor (basic_exec.c) ---- */

void exec_tokens(struct token *t);
void run_program(void);
void list_program(void);
void collect_data(void);
int  read_line(char *buf, int max);
double parse_number_string(const char *buf);

/* ---- Bump allocator (basic_vars.c) ---- */

void   basic_heap_init(void);
void  *basic_alloc(size_t size);
void   basic_alloc_reset(void);
void   basic_alloc_set_watermark(void);
size_t basic_heap_free(void);

#endif
