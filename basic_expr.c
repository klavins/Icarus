#include "basic_internal.h"
#include "math.h"

/* ---- Expression evaluator ---- */

struct token *tok_pos;
int expr_overflow;  /* set to 1 on overflow, checked by executor */

/* Simple linear congruential PRNG -- seeded from PIT timer on first call */
static uint32_t rng_state;
static int rng_seeded;

static double basic_rnd(int n) {
    if (!rng_seeded) {
        /* Seed from PIT channel 0 counter -- different each boot */
        outb(0x43, 0x00); /* latch counter 0 */
        uint32_t lo = inb(0x40);
        uint32_t hi = inb(0x40);
        rng_state = (hi << 8) | lo | 0x10000;
        rng_seeded = 1;
    }
    rng_state = rng_state * 1103515245 + 12345;
    if (n > 1) {
        /* Return integer 0 to n-1 for compatibility with RND(100) etc. */
        return (double)((int)((rng_state >> 16) % (uint32_t)n));
    }
    /* Return 0.0 to ~1.0 */
    return (double)((rng_state >> 16) & 0x7FFF) / 32768.0;
}

static double parse_factor(void) {
    if (tok_pos->type == TOK_NUMBER) {
        double val = tok_pos->number_val;
        tok_pos++;
        return val;
    }
    if (tok_pos->type == TOK_IDENT) {
        const char *name = tok_pos->string_val;
        tok_pos++;

        /* Built-in values (no parentheses) */
        if (strcmp(name, "SCRW") == 0) return (double)gfx_width();
        if (strcmp(name, "SCRH") == 0) return (double)gfx_height();
        if (strcmp(name, "PI") == 0) return 3.14159265358979323846;
        if (strcmp(name, "RND") == 0 && tok_pos->type != TOK_LPAREN)
            return basic_rnd(0); /* RND without parens = 0.0-1.0 */

        /* Built-in functions (with parentheses) */
        if (tok_pos->type == TOK_LPAREN) {
            if (strcmp(name, "RND") == 0) {
                tok_pos++;
                double n = parse_expr();
                if (tok_pos->type == TOK_RPAREN) tok_pos++;
                return basic_rnd((int)n);
            }
            if (strcmp(name, "ABS") == 0) {
                tok_pos++;
                double n = parse_expr();
                if (tok_pos->type == TOK_RPAREN) tok_pos++;
                return n < 0 ? -n : n;
            }
            if (strcmp(name, "PEEK") == 0) {
                tok_pos++;
                double addr = parse_expr();
                if (tok_pos->type == TOK_RPAREN) tok_pos++;
                return (double)*((volatile uint8_t *)(uintptr_t)(int)addr);
            }
            if (strcmp(name, "SIN") == 0) {
                tok_pos++;
                double n = parse_expr();
                if (tok_pos->type == TOK_RPAREN) tok_pos++;
                return ksin(n);
            }
            if (strcmp(name, "COS") == 0) {
                tok_pos++;
                double n = parse_expr();
                if (tok_pos->type == TOK_RPAREN) tok_pos++;
                return kcos(n);
            }
            if (strcmp(name, "SQR") == 0) {
                tok_pos++;
                double n = parse_expr();
                if (tok_pos->type == TOK_RPAREN) tok_pos++;
                return ksqrt(n);
            }
            if (strcmp(name, "INT") == 0) {
                tok_pos++;
                double n = parse_expr();
                if (tok_pos->type == TOK_RPAREN) tok_pos++;
                return (double)(long long)n;
            }
            if (strcmp(name, "LEN") == 0) {
                tok_pos++;
                if (tok_pos->type == TOK_STRIDENT) {
                    int len = strlen(strvar_get(tok_pos->string_val));
                    tok_pos++;
                    if (tok_pos->type == TOK_RPAREN) tok_pos++;
                    return (double)len;
                }
                if (tok_pos->type == TOK_STRING) {
                    int len = strlen(tok_pos->string_val);
                    tok_pos++;
                    if (tok_pos->type == TOK_RPAREN) tok_pos++;
                    return (double)len;
                }
                if (tok_pos->type == TOK_RPAREN) tok_pos++;
                return 0.0;
            }

            /* Array access: A(expr) or A(expr, expr) */
            tok_pos++;
            int i1 = (int)parse_expr();
            int i2 = 0;
            if (tok_pos->type == TOK_COMMA) {
                tok_pos++;
                i2 = (int)parse_expr();
            }
            if (tok_pos->type == TOK_RPAREN)
                tok_pos++;
            return array_get(name, i1, i2);
        }
        return var_get(name);
    }
    if (tok_pos->type == TOK_LPAREN) {
        tok_pos++;
        double val = parse_expr();
        if (tok_pos->type == TOK_RPAREN)
            tok_pos++;
        return val;
    }
    if (tok_pos->type == TOK_MINUS) {
        tok_pos++;
        return -parse_factor();
    }
    return 0.0;
}

static double parse_term(void) {
    double val = parse_factor();
    while (tok_pos->type == TOK_STAR || tok_pos->type == TOK_SLASH) {
        enum token_type op = tok_pos->type;
        tok_pos++;
        double right = parse_factor();
        if (op == TOK_STAR)
            val = val * right;
        else if (right != 0.0)
            val = val / right;
        else {
            terminal_print("?DIVISION BY ZERO\n");
            expr_overflow = 1;
            return 0.0;
        }
    }
    return val;
}

double parse_expr(void) {
    expr_overflow = 0;
    double val = parse_term();
    while (tok_pos->type == TOK_PLUS || tok_pos->type == TOK_MINUS) {
        enum token_type op = tok_pos->type;
        tok_pos++;
        double right = parse_term();
        if (op == TOK_PLUS)
            val = val + right;
        else
            val = val - right;
    }
    return val;
}

static int is_comparison(enum token_type t) {
    return t == TOK_EQUAL || t == TOK_LT || t == TOK_GT ||
           t == TOK_LE || t == TOK_GE || t == TOK_NE;
}

double parse_condition(void) {
    double left = parse_expr();
    if (!is_comparison(tok_pos->type))
        return left;
    enum token_type op = tok_pos->type;
    tok_pos++;
    double right = parse_expr();
    switch (op) {
    case TOK_EQUAL: return left == right ? 1.0 : 0.0;
    case TOK_LT:    return left < right ? 1.0 : 0.0;
    case TOK_GT:    return left > right ? 1.0 : 0.0;
    case TOK_LE:    return left <= right ? 1.0 : 0.0;
    case TOK_GE:    return left >= right ? 1.0 : 0.0;
    case TOK_NE:    return left != right ? 1.0 : 0.0;
    default:        return 0.0;
    }
}
