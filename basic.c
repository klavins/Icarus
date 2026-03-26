#include "basic.h"
#include "vga.h"
#include "klib.h"
#include "keyboard.h"
#include "speaker.h"
#include "io.h"
#include "fs.h"
#include "graphics.h"

/* Atari BASIC pitch to approximate Hz.
   Atari: freq = 63920 / (2 * (pitch + 1))
   pitch 0 = ~31960 Hz, pitch 255 = ~125 Hz */
static int atari_pitch_to_hz(int pitch) {
    if (pitch < 0) pitch = 0;
    if (pitch > 255) pitch = 255;
    return 63920 / (2 * (pitch + 1));
}

static int is_alpha(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

static int is_alnum(char c) {
    return is_alpha(c) || is_digit(c);
}

static char to_upper(char c) {
    if (c >= 'a' && c <= 'z')
        return c - 32;
    return c;
}

/* ---- Variables (name table) ---- */

#define MAX_VARS 64
#define MAX_VAR_NAME 16
#define MAX_STR_LEN 128
#define MAX_ARRAY_SIZE 1024

static struct {
    char name[MAX_VAR_NAME];
    int  val;
} num_vars[MAX_VARS];
static int num_var_count;

static int var_find(const char *name) {
    for (int i = 0; i < num_var_count; i++)
        if (strcmp(num_vars[i].name, name) == 0)
            return i;
    return -1;
}

static int var_get(const char *name) {
    int i = var_find(name);
    if (i >= 0) return num_vars[i].val;
    return 0;
}

static void var_set(const char *name, int val) {
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
static int str_var_count;

static int strvar_find(const char *name) {
    for (int i = 0; i < str_var_count; i++)
        if (strcmp(str_vars[i].name, name) == 0)
            return i;
    return -1;
}

static const char *strvar_get(const char *name) {
    int i = strvar_find(name);
    if (i >= 0) return str_vars[i].val;
    return "";
}

static void strvar_set(const char *name, const char *val) {
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

static void strvar_dim(const char *name, int size) {
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
    int  vals[MAX_ARRAY_SIZE];
} arrays[MAX_VARS];
static int array_count;

static int array_find(const char *name) {
    for (int i = 0; i < array_count; i++)
        if (strcmp(arrays[i].name, name) == 0)
            return i;
    return -1;
}

static void array_dim(const char *name, int s1, int s2) {
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
    memset(arrays[i].vals, 0, total * sizeof(int));
}

static int array_get(const char *name, int i1, int i2) {
    int a = array_find(name);
    if (a < 0 || !arrays[a].dimmed) {
        terminal_print("?ARRAY NOT DIMMED\n");
        return 0;
    }
    int idx;
    if (arrays[a].size2 > 0) {
        if (i1 < 0 || i1 >= arrays[a].size1 || i2 < 0 || i2 >= arrays[a].size2) {
            terminal_print("?BAD SUBSCRIPT\n");
            return 0;
        }
        idx = i1 * arrays[a].size2 + i2;
    } else {
        if (i1 < 0 || i1 >= arrays[a].size1) {
            terminal_print("?BAD SUBSCRIPT\n");
            return 0;
        }
        idx = i1;
    }
    return arrays[a].vals[idx];
}

static void array_set(const char *name, int i1, int i2, int val) {
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

/* ---- Program store ---- */

#define MAX_LINES 256
#define MAX_LINE_LEN 80

static struct {
    int linenum;
    char text[MAX_LINE_LEN];
} program[MAX_LINES];
static int program_count;

static int program_find(int linenum) {
    for (int i = 0; i < program_count; i++)
        if (program[i].linenum == linenum)
            return i;
    return -1;
}

static void program_insert(int linenum, const char *text) {
    int idx = program_find(linenum);
    if (idx >= 0) {
        /* Replace existing line */
        strcpy(program[idx].text, text);
        return;
    }
    if (program_count >= MAX_LINES) {
        terminal_print("?PROGRAM FULL\n");
        return;
    }
    /* Insert in sorted order */
    int i = program_count;
    while (i > 0 && program[i - 1].linenum > linenum) {
        program[i] = program[i - 1];
        i--;
    }
    program[i].linenum = linenum;
    strcpy(program[i].text, text);
    program_count++;
}

/* ---- FOR/NEXT stack ---- */

#define MAX_FOR_DEPTH 8

static struct {
    char var[MAX_VAR_NAME];
    int limit;
    int step;
    int program_idx;  /* index into program[] to loop back to */
} for_stack[MAX_FOR_DEPTH];
static int for_depth;

/* ---- GOSUB stack ---- */

#define MAX_GOSUB_DEPTH 16
static int gosub_stack[MAX_GOSUB_DEPTH];
static int gosub_depth;

/* ---- DATA/READ ---- */

#define MAX_DATA 1024

struct data_item {
    int is_string;
    int number_val;
    char string_val[MAX_TOKEN_LEN];
};

static struct data_item data_store[MAX_DATA];
static int data_count;
static int data_ptr;

/* Scan all program lines for DATA statements and collect items */
static void collect_data(void) {
    data_count = 0;
    data_ptr = 0;
    for (int i = 0; i < program_count && data_count < MAX_DATA; i++) {
        struct token tokens[MAX_TOKENS];
        if (basic_tokenize(program[i].text, tokens, MAX_TOKENS) < 0)
            continue;
        if (tokens[0].type != TOK_DATA)
            continue;
        int t = 1;
        while (tokens[t].type != TOK_EOL && data_count < MAX_DATA) {
            if (tokens[t].type == TOK_STRING) {
                data_store[data_count].is_string = 1;
                data_store[data_count].number_val = 0;
                strcpy(data_store[data_count].string_val, tokens[t].string_val);
                data_count++;
                t++;
            } else if (tokens[t].type == TOK_NUMBER) {
                data_store[data_count].is_string = 0;
                data_store[data_count].number_val = tokens[t].number_val;
                data_store[data_count].string_val[0] = '\0';
                data_count++;
                t++;
            } else if (tokens[t].type == TOK_MINUS && tokens[t+1].type == TOK_NUMBER) {
                data_store[data_count].is_string = 0;
                data_store[data_count].number_val = -tokens[t+1].number_val;
                data_store[data_count].string_val[0] = '\0';
                data_count++;
                t += 2;
            } else {
                t++;
            }
            if (tokens[t].type == TOK_COMMA)
                t++;
        }
    }
}

/* ---- Keywords ---- */

static enum token_type keyword_type(const char *word) {
    if (strcmp(word, "REM") == 0)   return TOK_REM;
    if (strcmp(word, "PRINT") == 0) return TOK_PRINT;
    if (strcmp(word, "LET") == 0)   return TOK_LET;
    if (strcmp(word, "DIM") == 0)   return TOK_DIM;
    if (strcmp(word, "IF") == 0)    return TOK_IF;
    if (strcmp(word, "THEN") == 0)  return TOK_THEN;
    if (strcmp(word, "FOR") == 0)   return TOK_FOR;
    if (strcmp(word, "TO") == 0)    return TOK_TO;
    if (strcmp(word, "STEP") == 0)  return TOK_STEP;
    if (strcmp(word, "NEXT") == 0)  return TOK_NEXT;
    if (strcmp(word, "GOTO") == 0)   return TOK_GOTO;
    if (strcmp(word, "GOSUB") == 0)  return TOK_GOSUB;
    if (strcmp(word, "RETURN") == 0) return TOK_RETURN;
    if (strcmp(word, "ON") == 0)     return TOK_ON;
    if (strcmp(word, "READ") == 0)   return TOK_READ;
    if (strcmp(word, "DATA") == 0)   return TOK_DATA;
    if (strcmp(word, "RESTORE") == 0) return TOK_RESTORE;
    if (strcmp(word, "RUN") == 0)   return TOK_RUN;
    if (strcmp(word, "LIST") == 0)  return TOK_LIST;
    if (strcmp(word, "CLR") == 0)   return TOK_CLR;
    if (strcmp(word, "QUIT") == 0)   return TOK_QUIT;
    if (strcmp(word, "SAVE") == 0)   return TOK_SAVE;
    if (strcmp(word, "LOAD") == 0)   return TOK_LOAD;
    if (strcmp(word, "DIR") == 0)    return TOK_DIR;
    if (strcmp(word, "DELETE") == 0)  return TOK_DELETE;
    if (strcmp(word, "FORMAT") == 0)  return TOK_FORMAT;
    if (strcmp(word, "DOS") == 0)     return TOK_DOS;
    if (strcmp(word, "COLOR") == 0) return TOK_COLOR;
    if (strcmp(word, "SOUND") == 0)    return TOK_SOUND;
    if (strcmp(word, "GRAPHICS") == 0) return TOK_GRAPHICS;
    if (strcmp(word, "PLOT") == 0)     return TOK_PLOT;
    if (strcmp(word, "DRAWTO") == 0)   return TOK_DRAWTO;
    if (strcmp(word, "INPUT") == 0)    return TOK_INPUT;
    if (strcmp(word, "PAUSE") == 0)    return TOK_PAUSE;
    if (strcmp(word, "POS") == 0)      return TOK_POS;
    if (strcmp(word, "TEXT") == 0)     return TOK_TEXT;
    return TOK_IDENT;
}

/* ---- Expression evaluator ---- */

static struct token *tok_pos;

static int parse_expr(void);

static int parse_factor(void) {
    if (tok_pos->type == TOK_NUMBER) {
        int val = tok_pos->number_val;
        tok_pos++;
        return val;
    }
    if (tok_pos->type == TOK_IDENT) {
        const char *name = tok_pos->string_val;
        tok_pos++;

        /* Built-in functions */
        if (strcmp(name, "SCRW") == 0) return gfx_width();
        if (strcmp(name, "SCRH") == 0) return gfx_height();

        if (tok_pos->type == TOK_LPAREN) {
            /* Array access: A(expr) or A(expr, expr) */
            tok_pos++;
            int i1 = parse_expr();
            int i2 = 0;
            if (tok_pos->type == TOK_COMMA) {
                tok_pos++;
                i2 = parse_expr();
            }
            if (tok_pos->type == TOK_RPAREN)
                tok_pos++;
            return array_get(name, i1, i2);
        }
        return var_get(name);
    }
    if (tok_pos->type == TOK_LPAREN) {
        tok_pos++;
        int val = parse_expr();
        if (tok_pos->type == TOK_RPAREN)
            tok_pos++;
        return val;
    }
    if (tok_pos->type == TOK_MINUS) {
        tok_pos++;
        return -parse_factor();
    }
    return 0;
}

static int parse_term(void) {
    int val = parse_factor();
    while (tok_pos->type == TOK_STAR || tok_pos->type == TOK_SLASH) {
        enum token_type op = tok_pos->type;
        tok_pos++;
        int right = parse_factor();
        if (op == TOK_STAR)
            val *= right;
        else if (right != 0)
            val /= right;
    }
    return val;
}

static int parse_expr(void) {
    int val = parse_term();
    while (tok_pos->type == TOK_PLUS || tok_pos->type == TOK_MINUS) {
        enum token_type op = tok_pos->type;
        tok_pos++;
        int right = parse_term();
        if (op == TOK_PLUS)
            val += right;
        else
            val -= right;
    }
    return val;
}

static int is_comparison(enum token_type t) {
    return t == TOK_EQUAL || t == TOK_LT || t == TOK_GT ||
           t == TOK_LE || t == TOK_GE || t == TOK_NE;
}

static int parse_condition(void) {
    int left = parse_expr();
    if (!is_comparison(tok_pos->type))
        return left;
    enum token_type op = tok_pos->type;
    tok_pos++;
    int right = parse_expr();
    switch (op) {
    case TOK_EQUAL: return left == right;
    case TOK_LT:    return left < right;
    case TOK_GT:    return left > right;
    case TOK_LE:    return left <= right;
    case TOK_GE:    return left >= right;
    case TOK_NE:    return left != right;
    default:        return 0;
    }
}

/* ---- Tokenizer ---- */

int basic_tokenize(const char *input, struct token *tokens, int max) {
    int count = 0;
    const char *p = input;

    while (*p && count < max - 1) {
        while (*p == ' ' || *p == '\t')
            p++;

        if (*p == '\0')
            break;

        struct token *t = &tokens[count];
        t->number_val = 0;
        t->string_val[0] = '\0';

        if (is_digit(*p)) {
            t->type = TOK_NUMBER;
            int val = 0;
            while (is_digit(*p))
                val = val * 10 + (*p++ - '0');
            t->number_val = val;
        } else if (is_alpha(*p)) {
            int i = 0;
            while (is_alnum(*p) && i < MAX_TOKEN_LEN - 1)
                t->string_val[i++] = to_upper(*p++);
            t->string_val[i] = '\0';
            if (*p == '$') {
                /* String variable: single letter + $ */
                t->type = TOK_STRIDENT;
                p++;
            } else {
                t->type = keyword_type(t->string_val);
                if (t->type == TOK_REM) {
                    count++;
                    goto done; /* skip rest of line */
                }
            }
        } else if (*p == '"') {
            p++;
            int i = 0;
            while (*p && *p != '"' && i < MAX_TOKEN_LEN - 1)
                t->string_val[i++] = *p++;
            t->string_val[i] = '\0';
            if (*p == '"')
                p++;
            t->type = TOK_STRING;
        } else if (*p == '<') {
            p++;
            if (*p == '=')      { t->type = TOK_LE; p++; }
            else if (*p == '>') { t->type = TOK_NE; p++; }
            else                { t->type = TOK_LT; }
        } else if (*p == '>') {
            p++;
            if (*p == '=')      { t->type = TOK_GE; p++; }
            else                { t->type = TOK_GT; }
        } else {
            switch (*p) {
            case '+': t->type = TOK_PLUS;   break;
            case '-': t->type = TOK_MINUS;  break;
            case '*': t->type = TOK_STAR;   break;
            case '/': t->type = TOK_SLASH;  break;
            case '=': t->type = TOK_EQUAL;  break;
            case '(': t->type = TOK_LPAREN; break;
            case ')': t->type = TOK_RPAREN; break;
            case ',': t->type = TOK_COMMA;  break;
            default:
                terminal_printf("?ILLEGAL CHARACTER: %c\n", *p);
                return -1;
            }
            p++;
        }
        count++;
    }

done:
    tokens[count].type = TOK_EOL;
    tokens[count].string_val[0] = '\0';
    tokens[count].number_val = 0;
    count++;

    return count;
}

/* ---- Executor ---- */

/* Used by RUN to signal jumps */
static int running;
static int next_line_idx;

static void dos_menu(void);

static void exec_tokens(struct token *t) {
    if (t->type == TOK_EOL)
        return;

    if (t->type == TOK_PRINT) {
        tok_pos = t + 1;
        while (tok_pos->type != TOK_EOL) {
            if (tok_pos->type == TOK_STRING) {
                terminal_print(tok_pos->string_val);
                tok_pos++;
            } else if (tok_pos->type == TOK_STRIDENT) {
                terminal_print(strvar_get(tok_pos->string_val));
                tok_pos++;
            } else {
                int val = parse_expr();
                terminal_printf("%d", val);
            }
            if (tok_pos->type == TOK_COMMA)
                tok_pos++;
        }
        terminal_print("\n");
    } else if (t->type == TOK_DIM) {
        /* DIM A$(20) or DIM A(10) */
        t++;
        if (t->type == TOK_STRIDENT) {
            const char *name = t->string_val;
            t++;
            if (t->type == TOK_LPAREN) {
                t++;
                tok_pos = t;
                int size = parse_expr();
                strvar_dim(name, size);
            } else {
                terminal_print("?SYNTAX ERROR\n");
            }
        } else if (t->type == TOK_IDENT) {
            const char *name = t->string_val;
            t++;
            if (t->type == TOK_LPAREN) {
                t++;
                tok_pos = t;
                int s1 = parse_expr();
                int s2 = 0;
                if (tok_pos->type == TOK_COMMA) {
                    tok_pos++;
                    s2 = parse_expr();
                }
                if (tok_pos->type == TOK_RPAREN)
                    tok_pos++;
                array_dim(name, s1, s2);
            } else {
                terminal_print("?SYNTAX ERROR\n");
            }
        } else {
            terminal_print("?SYNTAX ERROR\n");
        }
    } else if (t->type == TOK_LET) {
        t++;
        if (t->type == TOK_STRIDENT && t[1].type == TOK_EQUAL) {
            /* LET A$ = "hello" or LET A$ = B$ */
            const char *name = t->string_val;
            t += 2;
            if (t->type == TOK_STRING) {
                strvar_set(name, t->string_val);
            } else if (t->type == TOK_STRIDENT) {
                strvar_set(name, strvar_get(t->string_val));
            } else {
                terminal_print("?TYPE MISMATCH\n");
            }
        } else if (t->type == TOK_IDENT && t[1].type == TOK_LPAREN) {
            /* LET A(expr) = expr  or  LET A(expr, expr) = expr */
            const char *name = t->string_val;
            tok_pos = t + 2;
            int i1 = parse_expr();
            int i2 = 0;
            if (tok_pos->type == TOK_COMMA) {
                tok_pos++;
                i2 = parse_expr();
            }
            if (tok_pos->type == TOK_RPAREN)
                tok_pos++;
            if (tok_pos->type != TOK_EQUAL) {
                terminal_print("?SYNTAX ERROR\n");
                return;
            }
            tok_pos++;
            int val = parse_expr();
            array_set(name, i1, i2, val);
        } else if (t->type == TOK_IDENT && t[1].type == TOK_EQUAL) {
            const char *name = t->string_val;
            tok_pos = t + 2;
            int val = parse_expr();
            var_set(name, val);
        } else {
            terminal_print("?SYNTAX ERROR\n");
        }
    } else if (t->type == TOK_IF) {
        t++;
        tok_pos = t;
        int cond = parse_condition();
        if (tok_pos->type == TOK_THEN) {
            tok_pos++;
            if (cond)
                exec_tokens(tok_pos);
        } else {
            terminal_print("?SYNTAX ERROR: MISSING THEN\n");
        }
    } else if (t->type == TOK_FOR) {
        /* FOR X = start TO end [STEP n] */
        t++;
        if (t->type != TOK_IDENT) {
            terminal_print("?SYNTAX ERROR\n");
            return;
        }
        const char *var = t->string_val;
        t++;
        if (t->type != TOK_EQUAL) {
            terminal_print("?SYNTAX ERROR\n");
            return;
        }
        t++;
        tok_pos = t;
        int start = parse_expr();
        if (tok_pos->type != TOK_TO) {
            terminal_print("?SYNTAX ERROR: MISSING TO\n");
            return;
        }
        tok_pos++;
        int limit = parse_expr();
        int step = 1;
        if (tok_pos->type == TOK_STEP) {
            tok_pos++;
            step = parse_expr();
        }
        var_set(var, start);
        if (for_depth >= MAX_FOR_DEPTH) {
            terminal_print("?FOR OVERFLOW\n");
            return;
        }
        strncpy(for_stack[for_depth].var, var, 15); for_stack[for_depth].var[15] = '\0';
        for_stack[for_depth].limit = limit;
        for_stack[for_depth].step = step;
        for_stack[for_depth].program_idx = next_line_idx;
        for_depth++;
    } else if (t->type == TOK_NEXT) {
        t++;
        if (t->type != TOK_IDENT || for_depth == 0) {
            terminal_print("?NEXT WITHOUT FOR\n");
            return;
        }
        const char *var = t->string_val;
        /* Find matching FOR on stack */
        int fi = for_depth - 1;
        while (fi >= 0 && strcmp(for_stack[fi].var, var) != 0)
            fi--;
        if (fi < 0) {
            terminal_print("?NEXT WITHOUT FOR\n");
            return;
        }
        int val = var_get(var) + for_stack[fi].step;
        var_set(var, val);
        int done;
        if (for_stack[fi].step > 0)
            done = val > for_stack[fi].limit;
        else
            done = val < for_stack[fi].limit;
        if (done) {
            /* Loop finished — pop stack */
            for_depth = fi;
        } else {
            /* Jump back to line after FOR */
            next_line_idx = for_stack[fi].program_idx;
        }
    } else if (t->type == TOK_GOTO) {
        t++;
        tok_pos = t;
        int target = parse_expr();
        int idx = program_find(target);
        if (idx < 0) {
            terminal_print("?UNDEFINED LINE\n");
            running = 0;
        } else {
            next_line_idx = idx;
        }
    } else if (t->type == TOK_GOSUB) {
        t++;
        tok_pos = t;
        int target = parse_expr();
        int idx = program_find(target);
        if (idx < 0) {
            terminal_print("?UNDEFINED LINE\n");
            running = 0;
        } else {
            if (gosub_depth >= MAX_GOSUB_DEPTH) {
                terminal_print("?GOSUB OVERFLOW\n");
                running = 0;
            } else {
                gosub_stack[gosub_depth++] = next_line_idx;
                next_line_idx = idx;
            }
        }
    } else if (t->type == TOK_RETURN) {
        if (gosub_depth == 0) {
            terminal_print("?RETURN WITHOUT GOSUB\n");
            running = 0;
        } else {
            next_line_idx = gosub_stack[--gosub_depth];
        }
    } else if (t->type == TOK_ON) {
        /* ON expr GOTO/GOSUB line1, line2, line3, ... */
        t++;
        tok_pos = t;
        int val = parse_expr();
        int is_gosub;
        if (tok_pos->type == TOK_GOTO) {
            is_gosub = 0;
        } else if (tok_pos->type == TOK_GOSUB) {
            is_gosub = 1;
        } else {
            terminal_print("?SYNTAX ERROR: EXPECTED GOTO OR GOSUB\n");
            return;
        }
        tok_pos++;
        /* Collect line numbers, pick the val-th one (1-based) */
        int n = 0;
        int target = -1;
        while (tok_pos->type != TOK_EOL) {
            int linenum = parse_expr();
            n++;
            if (n == val)
                target = linenum;
            if (tok_pos->type == TOK_COMMA)
                tok_pos++;
            else
                break;
        }
        if (val < 1 || val > n) {
            /* Out of range — fall through to next line (Atari behavior) */
        } else {
            int idx = program_find(target);
            if (idx < 0) {
                terminal_print("?UNDEFINED LINE\n");
                running = 0;
            } else {
                if (is_gosub) {
                    if (gosub_depth >= MAX_GOSUB_DEPTH) {
                        terminal_print("?GOSUB OVERFLOW\n");
                        running = 0;
                    } else {
                        gosub_stack[gosub_depth++] = next_line_idx;
                        next_line_idx = idx;
                    }
                } else {
                    next_line_idx = idx;
                }
            }
        }
    } else if (t->type == TOK_REM) {
        /* Comment — skip */
    } else if (t->type == TOK_DATA) {
        /* DATA lines are handled by collect_data, skip at runtime */
    } else if (t->type == TOK_READ) {
        /* READ X, Y, A$, ... */
        t++;
        while (t->type != TOK_EOL) {
            if (data_ptr >= data_count) {
                terminal_print("?OUT OF DATA\n");
                running = 0;
                return;
            }
            if (t->type == TOK_IDENT && t[1].type == TOK_LPAREN) {
                /* READ A(expr) or READ A(expr, expr) */
                if (data_store[data_ptr].is_string) {
                    terminal_print("?TYPE MISMATCH IN READ\n");
                    running = 0;
                    return;
                }
                const char *name = t->string_val;
                tok_pos = t + 2;
                int i1 = parse_expr();
                int i2 = 0;
                if (tok_pos->type == TOK_COMMA) {
                    tok_pos++;
                    i2 = parse_expr();
                }
                if (tok_pos->type == TOK_RPAREN)
                    tok_pos++;
                array_set(name, i1, i2, data_store[data_ptr].number_val);
                data_ptr++;
                t = tok_pos;
            } else if (t->type == TOK_IDENT) {
                if (data_store[data_ptr].is_string) {
                    terminal_print("?TYPE MISMATCH IN READ\n");
                    running = 0;
                    return;
                }
                var_set(t->string_val, data_store[data_ptr].number_val);
                data_ptr++;
                t++;
            } else if (t->type == TOK_STRIDENT) {
                if (!data_store[data_ptr].is_string) {
                    terminal_print("?TYPE MISMATCH IN READ\n");
                    running = 0;
                    return;
                }
                strvar_set(t->string_val, data_store[data_ptr].string_val);
                data_ptr++;
                t++;
            } else {
                terminal_print("?SYNTAX ERROR\n");
                return;
            }
            if (t->type == TOK_COMMA)
                t++;
        }
    } else if (t->type == TOK_RESTORE) {
        data_ptr = 0;
    } else if (t->type == TOK_COLOR) {
        t++;
        tok_pos = t;
        int val = parse_expr();
        if (gfx_get_mode() >= 1)
            gfx_set_color(val);
        else
            terminal_setcolor(val, VGA_BLACK);
    } else if (t->type == TOK_SOUND) {
        /* SOUND voice, pitch, distortion, volume */
        t++;
        tok_pos = t;
        int voice = parse_expr();
        if (tok_pos->type == TOK_COMMA) tok_pos++;
        int pitch = parse_expr();
        if (tok_pos->type == TOK_COMMA) tok_pos++;
        int dist = parse_expr();
        (void)dist;  /* PC speaker has no distortion control */
        if (tok_pos->type == TOK_COMMA) tok_pos++;
        int volume = parse_expr();
        (void)voice; /* only one speaker */
        if (volume == 0) {
            speaker_off();
        } else {
            int hz = atari_pitch_to_hz(pitch);
            speaker_on(hz);
        }
    } else if (t->type == TOK_GRAPHICS) {
        /* GRAPHICS 0 or GRAPHICS 1 */
        t++;
        tok_pos = t;
        int mode = parse_expr();
        gfx_set_mode(mode);
    } else if (t->type == TOK_PLOT) {
        /* PLOT x, y */
        t++;
        tok_pos = t;
        int x = parse_expr();
        if (tok_pos->type == TOK_COMMA) tok_pos++;
        int y = parse_expr();
        gfx_plot(x, y);
    } else if (t->type == TOK_DRAWTO) {
        /* DRAWTO x, y */
        t++;
        tok_pos = t;
        int x = parse_expr();
        if (tok_pos->type == TOK_COMMA) tok_pos++;
        int y = parse_expr();
        gfx_drawto(x, y);
    } else if (t->type == TOK_INPUT) {
        /* INPUT X  or  INPUT "prompt", X  or  INPUT A$ */
        t++;
        /* Optional string prompt */
        if (t->type == TOK_STRING) {
            terminal_print(t->string_val);
            t++;
            if (t->type == TOK_COMMA) t++;
        }
        if (t->type == TOK_STRIDENT) {
            /* String input */
            const char *name = t->string_val;
            char buf[MAX_STR_LEN];
            int pos = 0;
            for (;;) {
                char c = keyboard_poll();
                if (c == '\n') break;
                if (c == '\b' && pos > 0) {
                    pos--;
                    terminal_putchar('\b');
                    continue;
                }
                if (c && pos < MAX_STR_LEN - 1) {
                    buf[pos++] = c;
                    terminal_putchar(c);
                }
            }
            buf[pos] = '\0';
            terminal_putchar('\n');
            strvar_set(name, buf);
        } else if (t->type == TOK_IDENT) {
            /* Numeric input */
            const char *name = t->string_val;
            char buf[32];
            int pos = 0;
            for (;;) {
                char c = keyboard_poll();
                if (c == '\n') break;
                if (c == '\b' && pos > 0) {
                    pos--;
                    terminal_putchar('\b');
                    continue;
                }
                if (c && pos < 31) {
                    buf[pos++] = c;
                    terminal_putchar(c);
                }
            }
            buf[pos] = '\0';
            terminal_putchar('\n');
            /* Parse the number */
            int val = 0;
            int neg = 0;
            int i = 0;
            if (buf[0] == '-') { neg = 1; i = 1; }
            while (buf[i] >= '0' && buf[i] <= '9')
                val = val * 10 + (buf[i++] - '0');
            if (neg) val = -val;
            var_set(name, val);
        } else {
            terminal_print("?SYNTAX ERROR\n");
        }
    } else if (t->type == TOK_PAUSE) {
        /* PAUSE — wait for any keypress */
        keyboard_poll();
    } else if (t->type == TOK_POS) {
        /* POS x, y */
        t++;
        tok_pos = t;
        int x = parse_expr();
        if (tok_pos->type == TOK_COMMA) tok_pos++;
        int y = parse_expr();
        gfx_pos(x, y);
    } else if (t->type == TOK_TEXT) {
        /* TEXT "string" or TEXT A$ */
        t++;
        if (t->type == TOK_STRING) {
            gfx_text(t->string_val);
        } else if (t->type == TOK_STRIDENT) {
            gfx_text(strvar_get(t->string_val));
        } else {
            /* Evaluate as expression and print the number */
            tok_pos = t;
            int val = parse_expr();
            char buf[32];
            sprintf(buf, "%d", val);
            gfx_text(buf);
        }
    } else {
        terminal_print("?SYNTAX ERROR\n");
    }
}

static void run_program(void) {
    running = 1;
    for_depth = 0;
    gosub_depth = 0;
    next_line_idx = 0;
    collect_data();

    while (running && next_line_idx < program_count) {
        if (keyboard_escape_pressed()) {
            terminal_print("\n?BREAK\n");
            running = 0;
            break;
        }

        int idx = next_line_idx;
        next_line_idx = idx + 1;

        struct token tokens[MAX_TOKENS];
        if (basic_tokenize(program[idx].text, tokens, MAX_TOKENS) < 0) {
            terminal_printf("?ERROR IN LINE %d\n", program[idx].linenum);
            running = 0;
            break;
        }

        exec_tokens(&tokens[0]);
    }
    running = 0;
}

static void list_program(void) {
    for (int i = 0; i < program_count; i++)
        terminal_printf("%d %s\n", program[i].linenum, program[i].text);
}

void basic_init(void) {
    program_count = 0;
    for_depth = 0;
    gosub_depth = 0;
    num_var_count = 0;
    str_var_count = 0;
    array_count = 0;
}

void basic_exec(const char *line) {
    struct token tokens[MAX_TOKENS];
    int count = basic_tokenize(line, tokens, MAX_TOKENS);

    if (count < 0)
        return;
    if (count < 1 || tokens[0].type == TOK_EOL)
        return;

    /* RUN command */
    if (tokens[0].type == TOK_RUN) {
        run_program();
        return;
    }

    /* LIST command */
    if (tokens[0].type == TOK_LIST) {
        list_program();
        return;
    }

    /* CLR command — clear program */
    if (tokens[0].type == TOK_CLR) {
        basic_init();
        terminal_print("OK\n");
        return;
    }

    /* QUIT command — shutdown */
    if (tokens[0].type == TOK_QUIT) {
        terminal_print("SHUTTING DOWN...\n");
        /* Try multiple QEMU shutdown methods */
        outw(0x604, 0x2000);   /* PIIX4 ACPI (BIOS mode) */
        outw(0xB004, 0x2000);  /* Bochs/old QEMU */
        outw(0x4004, 0x3400);  /* QEMU Q35/UEFI */
        /* Halt if none worked */
        asm volatile ("cli; hlt");
        return;
    }

    /* SAVE "filename" */
    if (tokens[0].type == TOK_SAVE) {
        if (tokens[1].type != TOK_STRING) {
            terminal_print("?SYNTAX ERROR\n");
            return;
        }
        static char savebuf[16384];
        int size = basic_program_serialize(savebuf, sizeof(savebuf));
        if (size <= 0) {
            terminal_print("?NO PROGRAM\n");
            return;
        }
        if (fs_save(tokens[1].string_val, savebuf, size) == 0)
            terminal_print("SAVED\n");
        return;
    }

    /* LOAD "filename" */
    if (tokens[0].type == TOK_LOAD) {
        if (tokens[1].type != TOK_STRING) {
            terminal_print("?SYNTAX ERROR\n");
            return;
        }
        static char loadbuf[16384];
        int size = fs_load(tokens[1].string_val, loadbuf, sizeof(loadbuf));
        if (size > 0) {
            basic_program_deserialize(loadbuf, size);
            terminal_print("LOADED\n");
        }
        return;
    }

    /* DIR */
    if (tokens[0].type == TOK_DIR) {
        fs_list();
        return;
    }

    /* DELETE "filename" */
    if (tokens[0].type == TOK_DELETE) {
        if (tokens[1].type != TOK_STRING) {
            terminal_print("?SYNTAX ERROR\n");
            return;
        }
        if (fs_delete(tokens[1].string_val) == 0)
            terminal_print("DELETED\n");
        return;
    }

    /* FORMAT */
    if (tokens[0].type == TOK_FORMAT) {
        terminal_print("FORMAT DISK - ARE YOU SURE? (Y/N) ");
        char c = 0;
        while (c != 'y' && c != 'Y' && c != 'n' && c != 'N')
            c = keyboard_poll();
        terminal_putchar(c);
        terminal_putchar('\n');
        if (c == 'y' || c == 'Y') {
            if (fs_format() == 0)
                terminal_print("FORMATTED\n");
            else
                terminal_print("?FORMAT ERROR\n");
        }
        return;
    }

    /* DOS — Atari-style disk utility menu */
    if (tokens[0].type == TOK_DOS) {
        dos_menu();
        return;
    }

    /* If the line starts with a number, store it as a program line */
    if (tokens[0].type == TOK_NUMBER) {
        int linenum = tokens[0].number_val;
        /* Find where the line number ends in the input */
        const char *p = line;
        while (*p == ' ') p++;
        while (*p >= '0' && *p <= '9') p++;
        while (*p == ' ') p++;
        if (*p == '\0') {
            /* Just a line number — delete the line */
            int idx = program_find(linenum);
            if (idx >= 0) {
                for (int i = idx; i < program_count - 1; i++)
                    program[i] = program[i + 1];
                program_count--;
            }
        } else {
            program_insert(linenum, p);
        }
        return;
    }

    /* Otherwise execute immediately */
    exec_tokens(&tokens[0]);
}

/* ---- Program serialization ---- */

int basic_program_serialize(char *buf, int max_size) {
    int pos = 0;
    for (int i = 0; i < program_count; i++) {
        char line[MAX_LINE_LEN + 16];
        /* Reconstruct: "linenum text\n" */
        int n = sprintf(line, "%d %s\n", program[i].linenum, program[i].text);
        if (pos + n >= max_size)
            break;
        memcpy(buf + pos, line, n);
        pos += n;
    }
    return pos;
}

int basic_program_deserialize(const char *buf, int size) {
    /* Clear current program */
    program_count = 0;

    int i = 0;
    while (i < size) {
        /* Extract one line */
        char line[MAX_LINE_LEN + 16];
        int j = 0;
        while (i < size && buf[i] != '\n' && j < (int)sizeof(line) - 1)
            line[j++] = buf[i++];
        line[j] = '\0';
        if (i < size && buf[i] == '\n')
            i++;

        if (j == 0) continue;

        /* Parse as a numbered line */
        basic_exec(line);
    }
    return 0;
}

/* ---- DOS Menu ---- */

static void dos_menu(void) {
    for (;;) {
        terminal_setcolor(VGA_YELLOW, VGA_BLACK);
        terminal_print("\n");
        terminal_print(" ================================\n");
        terminal_print("  ICARUS DOS\n");
        terminal_print(" ================================\n");
        terminal_setcolor(VGA_LCYAN, VGA_BLACK);
        terminal_print("  D - Directory\n");
        terminal_print("  L - Load file\n");
        terminal_print("  S - Save program\n");
        terminal_print("  E - Erase file\n");
        terminal_print("  F - Format disk\n");
        terminal_print("  B - Back to BASIC\n");
        terminal_print("\n");
        terminal_print("  Choice: ");

        char c = 0;
        while (!c) c = keyboard_poll();
        terminal_putchar(c);
        terminal_putchar('\n');

        if (c == 'b' || c == 'B')
            return;

        if (c == 'd' || c == 'D') {
            terminal_print("\n");
            fs_list();
        } else if (c == 'f' || c == 'F') {
            terminal_print(" Are you sure? (Y/N) ");
            char yn = 0;
            while (!yn) yn = keyboard_poll();
            terminal_putchar(yn);
            terminal_putchar('\n');
            if (yn == 'y' || yn == 'Y') {
                if (fs_format() == 0)
                    terminal_print(" Formatted.\n");
                else
                    terminal_print(" ?FORMAT ERROR\n");
            }
        } else if (c == 's' || c == 'S') {
            terminal_print(" Filename: ");
            char name[FS_NAME_LEN];
            int pos = 0;
            for (;;) {
                char k = keyboard_poll();
                if (k == '\n') break;
                if (k == '\b' && pos > 0) { pos--; terminal_putchar('\b'); continue; }
                if (k && pos < FS_NAME_LEN - 1) {
                    name[pos++] = k;
                    terminal_putchar(k);
                }
            }
            name[pos] = '\0';
            terminal_putchar('\n');
            if (pos > 0) {
                static char savebuf[16384];
                int size = basic_program_serialize(savebuf, sizeof(savebuf));
                if (size > 0) {
                    if (fs_save(name, savebuf, size) == 0)
                        terminal_print(" Saved.\n");
                } else {
                    terminal_print(" ?NO PROGRAM\n");
                }
            }
        } else if (c == 'l' || c == 'L') {
            terminal_print(" Filename: ");
            char name[FS_NAME_LEN];
            int pos = 0;
            for (;;) {
                char k = keyboard_poll();
                if (k == '\n') break;
                if (k == '\b' && pos > 0) { pos--; terminal_putchar('\b'); continue; }
                if (k && pos < FS_NAME_LEN - 1) {
                    name[pos++] = k;
                    terminal_putchar(k);
                }
            }
            name[pos] = '\0';
            terminal_putchar('\n');
            if (pos > 0) {
                static char loadbuf[16384];
                int size = fs_load(name, loadbuf, sizeof(loadbuf));
                if (size > 0) {
                    basic_program_deserialize(loadbuf, size);
                    terminal_print(" Loaded.\n");
                }
            }
        } else if (c == 'e' || c == 'E') {
            terminal_print(" Filename: ");
            char name[FS_NAME_LEN];
            int pos = 0;
            for (;;) {
                char k = keyboard_poll();
                if (k == '\n') break;
                if (k == '\b' && pos > 0) { pos--; terminal_putchar('\b'); continue; }
                if (k && pos < FS_NAME_LEN - 1) {
                    name[pos++] = k;
                    terminal_putchar(k);
                }
            }
            name[pos] = '\0';
            terminal_putchar('\n');
            if (pos > 0) {
                if (fs_delete(name) == 0)
                    terminal_print(" Deleted.\n");
            }
        }
    }
}
