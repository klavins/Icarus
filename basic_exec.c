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

void exec_tokens(struct token *t) {
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
            /* Loop finished -- pop stack */
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
            /* Out of range -- fall through to next line (Atari behavior) */
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
        /* Comment -- skip */
    } else if (t->type == TOK_POKE) {
        /* POKE address, value */
        t++;
        tok_pos = t;
        int addr = parse_expr();
        if (tok_pos->type == TOK_COMMA) tok_pos++;
        int val = parse_expr();
        *((volatile uint8_t *)(uintptr_t)addr) = (uint8_t)val;
    } else if (t->type == TOK_DELAY) {
        /* DELAY n -- wait for n milliseconds (timer runs at 200Hz = 5ms/tick) */
        t++;
        tok_pos = t;
        int n = parse_expr();
        gfx_present(); /* flush any drawing before waiting */
        volatile uint32_t *ticks = SYS_TICKS_PTR;
        uint32_t wait_ticks = (uint32_t)n / 5;
        if (wait_ticks < 1) wait_ticks = 1;
        uint32_t start = *ticks;
        while ((*ticks - start) < wait_ticks)
            asm volatile ("hlt");
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
        /* PAUSE -- flush display and wait for any keypress */
        gfx_present();
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
    }
    running = 0;
    keybuf_flush();
}

void list_program(void) {
    for (int i = 0; i < program_count; i++)
        terminal_printf("%d %s\n", program[i].linenum, program[i].text);
}
