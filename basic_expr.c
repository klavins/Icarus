#include "basic_internal.h"

/* ---- Expression evaluator ---- */

struct token *tok_pos;

/* Simple linear congruential PRNG -- seeded from PIT timer on first call */
static uint32_t rng_state;
static int rng_seeded;

static int basic_rnd(int n) {
    if (!rng_seeded) {
        /* Seed from PIT channel 0 counter -- different each boot */
        outb(0x43, 0x00); /* latch counter 0 */
        uint32_t lo = inb(0x40);
        uint32_t hi = inb(0x40);
        rng_state = (hi << 8) | lo | 0x10000;
        rng_seeded = 1;
    }
    rng_state = rng_state * 1103515245 + 12345;
    if (n <= 0) return 0;
    return (int)((rng_state >> 16) % (uint32_t)n);
}

static int parse_factor(void) {
    if (tok_pos->type == TOK_NUMBER) {
        int val = tok_pos->number_val;
        tok_pos++;
        return val;
    }
    if (tok_pos->type == TOK_IDENT) {
        const char *name = tok_pos->string_val;
        tok_pos++;

        /* Built-in values */
        if (strcmp(name, "SCRW") == 0) return gfx_width();
        if (strcmp(name, "SCRH") == 0) return gfx_height();

        /* Built-in functions */
        if (tok_pos->type == TOK_LPAREN) {
            if (strcmp(name, "RND") == 0) {
                tok_pos++;
                int n = parse_expr();
                if (tok_pos->type == TOK_RPAREN) tok_pos++;
                return basic_rnd(n);
            }
            if (strcmp(name, "ABS") == 0) {
                tok_pos++;
                int n = parse_expr();
                if (tok_pos->type == TOK_RPAREN) tok_pos++;
                return n < 0 ? -n : n;
            }
            if (strcmp(name, "PEEK") == 0) {
                tok_pos++;
                int addr = parse_expr();
                if (tok_pos->type == TOK_RPAREN) tok_pos++;
                return *((volatile uint8_t *)(uintptr_t)addr);
            }
            if (strcmp(name, "LEN") == 0) {
                tok_pos++;
                if (tok_pos->type == TOK_STRIDENT) {
                    int len = strlen(strvar_get(tok_pos->string_val));
                    tok_pos++;
                    if (tok_pos->type == TOK_RPAREN) tok_pos++;
                    return len;
                }
                if (tok_pos->type == TOK_STRING) {
                    int len = strlen(tok_pos->string_val);
                    tok_pos++;
                    if (tok_pos->type == TOK_RPAREN) tok_pos++;
                    return len;
                }
                if (tok_pos->type == TOK_RPAREN) tok_pos++;
                return 0;
            }

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

int parse_expr(void) {
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

int parse_condition(void) {
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
