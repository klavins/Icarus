#include "basic_internal.h"

/* ---- Executor state ---- */

int running;
int next_line_idx;

/* ---- FOR/NEXT stack ---- */

struct for_entry for_stack[MAX_FOR_DEPTH];
int for_depth;

/* ---- GOSUB stack ---- */

int gosub_stack[MAX_GOSUB_DEPTH];
int gosub_depth;

/* ---- DATA/READ ---- */

struct data_item data_store[MAX_DATA];
int data_count;
int data_ptr;

/* Scan all program lines for DATA statements and collect items */
void collect_data(void) {
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

/* Atari BASIC pitch to approximate Hz.
   Atari: freq = 63920 / (2 * (pitch + 1))
   pitch 0 = ~31960 Hz, pitch 255 = ~125 Hz */
static int atari_pitch_to_hz(int pitch) {
    if (pitch < 0) pitch = 0;
    if (pitch > 255) pitch = 255;
    return 63920 / (2 * (pitch + 1));
}

/* ---- Helpers ---- */

static int parse_int(void) {
    return (int)parse_expr();
}

double parse_number_string(const char *buf) {
    double val = 0;
    int neg = 0, i = 0;
    if (buf[0] == '-') { neg = 1; i = 1; }
    while (buf[i] >= '0' && buf[i] <= '9')
        val = val * 10 + (buf[i++] - '0');
    if (buf[i] == '.') {
        i++;
        double frac = 0.1;
        while (buf[i] >= '0' && buf[i] <= '9') {
            val += (buf[i++] - '0') * frac;
            frac *= 0.1;
        }
    }
    if (neg) val = -val;
    return val;
}

static void parse_xy(int *x, int *y) {
    *x = parse_int();
    if (tok_pos->type == TOK_COMMA) tok_pos++;
    *y = parse_int();
}

int read_line(char *buf, int max) {
    int pos = 0;
    for (;;) {
        char c = keyboard_poll();
        if (c == '\n') break;
        if (c == '\b' && pos > 0) { pos--; terminal_putchar('\b'); continue; }
        if (c && pos < max - 1) { buf[pos++] = c; terminal_putchar(c); }
    }
    buf[pos] = '\0';
    terminal_putchar('\n');
    return pos;
}

/* ---- Statement dispatcher ---- */

void exec_tokens(struct token *t) {
    if (t->type == TOK_EOL)
        return;

    switch (t->type) {

    case TOK_PRINT:
        tok_pos = t + 1;
        while (tok_pos->type != TOK_EOL) {
            if (tok_pos->type == TOK_STRING) {
                terminal_print(tok_pos->string_val);
                tok_pos++;
            } else if (tok_pos->type == TOK_STRIDENT) {
                terminal_print(strvar_get(tok_pos->string_val));
                tok_pos++;
            } else {
                double val = parse_expr();
                terminal_printf("%g", val);
            }
            if (tok_pos->type == TOK_COMMA)
                tok_pos++;
        }
        terminal_print("\n");
        break;

    case TOK_DIM:
        t++;
        if (t->type == TOK_STRIDENT) {
            const char *name = t->string_val;
            t++;
            if (t->type == TOK_LPAREN) {
                t++;
                tok_pos = t;
                int size = parse_int();
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
                int s1 = parse_int();
                int s2 = 0;
                if (tok_pos->type == TOK_COMMA) {
                    tok_pos++;
                    s2 = parse_int();
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
        break;

    case TOK_LET:
        t++;
        if (t->type == TOK_STRIDENT && t[1].type == TOK_EQUAL) {
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
            const char *name = t->string_val;
            tok_pos = t + 2;
            int i1 = parse_int();
            int i2 = 0;
            if (tok_pos->type == TOK_COMMA) {
                tok_pos++;
                i2 = parse_int();
            }
            if (tok_pos->type == TOK_RPAREN)
                tok_pos++;
            if (tok_pos->type != TOK_EQUAL) {
                terminal_print("?SYNTAX ERROR\n");
                return;
            }
            tok_pos++;
            double val = parse_expr();
            array_set(name, i1, i2, val);
        } else if (t->type == TOK_IDENT && t[1].type == TOK_EQUAL) {
            const char *name = t->string_val;
            tok_pos = t + 2;
            double val = parse_expr();
            var_set(name, val);
        } else {
            terminal_print("?SYNTAX ERROR\n");
        }
        break;

    case TOK_IF:
        t++;
        tok_pos = t;
        {
            double cond = parse_condition();
            if (tok_pos->type == TOK_THEN) {
                tok_pos++;
                if (cond != 0.0)
                    exec_tokens(tok_pos);
            } else {
                terminal_print("?SYNTAX ERROR: MISSING THEN\n");
            }
        }
        break;

    case TOK_FOR:
        t++;
        if (t->type != TOK_IDENT) {
            terminal_print("?SYNTAX ERROR\n");
            return;
        }
        {
            const char *var = t->string_val;
            t++;
            if (t->type != TOK_EQUAL) {
                terminal_print("?SYNTAX ERROR\n");
                return;
            }
            t++;
            tok_pos = t;
            double start = parse_expr();
            if (tok_pos->type != TOK_TO) {
                terminal_print("?SYNTAX ERROR: MISSING TO\n");
                return;
            }
            tok_pos++;
            double limit = parse_expr();
            double step = 1.0;
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
        }
        break;

    case TOK_NEXT:
        t++;
        if (t->type != TOK_IDENT || for_depth == 0) {
            terminal_print("?NEXT WITHOUT FOR\n");
            return;
        }
        {
            const char *var = t->string_val;
            int fi = for_depth - 1;
            while (fi >= 0 && strcmp(for_stack[fi].var, var) != 0)
                fi--;
            if (fi < 0) {
                terminal_print("?NEXT WITHOUT FOR\n");
                return;
            }
            double val = var_get(var) + for_stack[fi].step;
            var_set(var, val);
            int done;
            if (for_stack[fi].step > 0)
                done = val > for_stack[fi].limit;
            else
                done = val < for_stack[fi].limit;
            if (done) {
                for_depth = fi;
            } else {
                next_line_idx = for_stack[fi].program_idx;
            }
        }
        break;

    case TOK_GOTO:
        t++;
        tok_pos = t;
        {
            int target = parse_int();
            int idx = program_find(target);
            if (idx < 0) { terminal_print("?UNDEFINED LINE\n"); running = 0; }
            else next_line_idx = idx;
        }
        break;

    case TOK_GOSUB:
        t++;
        tok_pos = t;
        {
            int target = parse_int();
            int idx = program_find(target);
            if (idx < 0) { terminal_print("?UNDEFINED LINE\n"); running = 0; }
            else if (gosub_depth >= MAX_GOSUB_DEPTH) { terminal_print("?GOSUB OVERFLOW\n"); running = 0; }
            else { gosub_stack[gosub_depth++] = next_line_idx; next_line_idx = idx; }
        }
        break;

    case TOK_RETURN:
        if (gosub_depth == 0) { terminal_print("?RETURN WITHOUT GOSUB\n"); running = 0; }
        else next_line_idx = gosub_stack[--gosub_depth];
        break;

    case TOK_ON:
        t++;
        tok_pos = t;
        {
            int val = parse_int();
            int is_gosub;
            if (tok_pos->type == TOK_GOTO) is_gosub = 0;
            else if (tok_pos->type == TOK_GOSUB) is_gosub = 1;
            else { terminal_print("?SYNTAX ERROR: EXPECTED GOTO OR GOSUB\n"); return; }
            tok_pos++;
            int n = 0, target = -1;
            while (tok_pos->type != TOK_EOL) {
                int linenum = parse_int();
                n++;
                if (n == val) target = linenum;
                if (tok_pos->type == TOK_COMMA) tok_pos++;
                else break;
            }
            if (val >= 1 && val <= n) {
                int idx = program_find(target);
                if (idx < 0) { terminal_print("?UNDEFINED LINE\n"); running = 0; }
                else if (is_gosub) {
                    if (gosub_depth >= MAX_GOSUB_DEPTH) { terminal_print("?GOSUB OVERFLOW\n"); running = 0; }
                    else { gosub_stack[gosub_depth++] = next_line_idx; next_line_idx = idx; }
                } else {
                    next_line_idx = idx;
                }
            }
        }
        break;

    case TOK_REM:
        break;

    case TOK_POKE:
        t++;
        tok_pos = t;
        {
            int addr = parse_int();
            if (tok_pos->type == TOK_COMMA) tok_pos++;
            int pval = parse_int();
            *((volatile uint8_t *)(uintptr_t)addr) = (uint8_t)pval;
        }
        break;

    case TOK_DELAY:
        t++;
        tok_pos = t;
        {
            int n = parse_int();
            gfx_present();
            volatile uint32_t *ticks = SYS_TICKS_PTR;
            uint32_t wait_ticks = (uint32_t)n / 5;
            if (wait_ticks < 1) wait_ticks = 1;
            uint32_t start = *ticks;
            while ((*ticks - start) < wait_ticks)
                asm volatile ("hlt");
        }
        break;

    case TOK_DATA:
        break;

    case TOK_READ:
        t++;
        while (t->type != TOK_EOL) {
            if (data_ptr >= data_count) {
                terminal_print("?OUT OF DATA\n");
                running = 0;
                return;
            }
            if (t->type == TOK_IDENT && t[1].type == TOK_LPAREN) {
                if (data_store[data_ptr].is_string) { terminal_print("?TYPE MISMATCH IN READ\n"); running = 0; return; }
                const char *name = t->string_val;
                tok_pos = t + 2;
                int i1 = parse_int();
                int i2 = 0;
                if (tok_pos->type == TOK_COMMA) { tok_pos++; i2 = parse_int(); }
                if (tok_pos->type == TOK_RPAREN) tok_pos++;
                array_set(name, i1, i2, data_store[data_ptr].number_val);
                data_ptr++;
                t = tok_pos;
            } else if (t->type == TOK_IDENT) {
                if (data_store[data_ptr].is_string) { terminal_print("?TYPE MISMATCH IN READ\n"); running = 0; return; }
                var_set(t->string_val, data_store[data_ptr].number_val);
                data_ptr++;
                t++;
            } else if (t->type == TOK_STRIDENT) {
                if (!data_store[data_ptr].is_string) { terminal_print("?TYPE MISMATCH IN READ\n"); running = 0; return; }
                strvar_set(t->string_val, data_store[data_ptr].string_val);
                data_ptr++;
                t++;
            } else {
                terminal_print("?SYNTAX ERROR\n");
                return;
            }
            if (t->type == TOK_COMMA) t++;
        }
        break;

    case TOK_RESTORE:
        data_ptr = 0;
        break;

    case TOK_COLOR:
        t++;
        tok_pos = t;
        {
            int val = parse_int();
            if (gfx_get_mode() >= 1) gfx_set_color(val);
            else terminal_setcolor(val, VGA_BLACK);
        }
        break;

    case TOK_SOUND:
        t++;
        tok_pos = t;
        {
            int voice = parse_int();
            if (tok_pos->type == TOK_COMMA) tok_pos++;
            int pitch = parse_int();
            if (tok_pos->type == TOK_COMMA) tok_pos++;
            int dist = parse_int();
            (void)dist;
            if (tok_pos->type == TOK_COMMA) tok_pos++;
            int volume = parse_int();
            (void)voice;
            if (volume == 0) speaker_off();
            else speaker_on(atari_pitch_to_hz(pitch));
        }
        break;

    case TOK_GRAPHICS:
        t++;
        tok_pos = t;
        gfx_set_mode(parse_int());
        break;

    case TOK_PLOT:
        t++;
        tok_pos = t;
        { int x, y; parse_xy(&x, &y); gfx_plot(x, y); }
        break;

    case TOK_DRAWTO:
        t++;
        tok_pos = t;
        { int x, y; parse_xy(&x, &y); gfx_drawto(x, y); }
        break;

    case TOK_INPUT:
        t++;
        if (t->type == TOK_STRING) {
            terminal_print(t->string_val);
            t++;
            if (t->type == TOK_COMMA) t++;
        }
        if (t->type == TOK_STRIDENT) {
            const char *name = t->string_val;
            char buf[MAX_STR_LEN];
            read_line(buf, MAX_STR_LEN);
            strvar_set(name, buf);
        } else if (t->type == TOK_IDENT) {
            const char *name = t->string_val;
            char buf[32];
            read_line(buf, 32);
            var_set(name, parse_number_string(buf));
        } else {
            terminal_print("?SYNTAX ERROR\n");
        }
        break;

    case TOK_PAUSE:
        gfx_present();
        keyboard_poll();
        break;

    case TOK_POS:
        t++;
        tok_pos = t;
        { int x, y; parse_xy(&x, &y); gfx_pos(x, y); }
        break;

    case TOK_TEXT:
        t++;
        if (t->type == TOK_STRING) {
            gfx_text(t->string_val);
        } else if (t->type == TOK_STRIDENT) {
            gfx_text(strvar_get(t->string_val));
        } else {
            tok_pos = t;
            double val = parse_expr();
            char buf[32];
            sprintf(buf, "%g", val);
            gfx_text(buf);
        }
        break;

    default:
        terminal_print("?SYNTAX ERROR\n");
        break;
    }
}

void run_program(void) {
    running = 1;
    for_depth = 0;
    gosub_depth = 0;
    next_line_idx = 0;
    collect_data();

    while (running && next_line_idx < program_count) {
        if (keyboard_escape_pressed()) {
            if (gfx_get_mode() >= 1)
                gfx_set_mode(0);
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

        if (expr_overflow) {
            terminal_printf("?OVERFLOW IN LINE %d\n", program[idx].linenum);
            running = 0;
            break;
        }
    }
    running = 0;
    keybuf_flush();
}

void list_program(void) {
    for (int i = 0; i < program_count; i++)
        terminal_printf("%d %s\n", program[i].linenum, program[i].text);
}
