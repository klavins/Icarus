/*
 * basic.c - BASIC interpreter core and interactive command loop
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

#include "basic_internal.h"
#include "klib.h"

#define BASIC_FILENAME_LEN 12

/* ---- Program store ---- */

struct program_line program[MAX_LINES];
int program_count;

int program_find(int linenum) {
    for (int i = 0; i < program_count; i++)
        if (program[i].linenum == linenum)
            return i;
    return -1;
}

static void program_insert(int linenum, const char *text) {
    int idx = program_find(linenum);
    if (idx >= 0) {
        /* Replace existing line */
        int len = strlen(text);
        program[idx].text = basic_alloc(len + 1);
        if (!program[idx].text) {
            os_print("?OUT OF MEMORY\n");
            return;
        }
        strcpy(program[idx].text, text);
        return;
    }
    if (program_count >= MAX_LINES) {
        os_print("?PROGRAM FULL\n");
        return;
    }
    /* Insert in sorted order */
    int i = program_count;
    while (i > 0 && program[i - 1].linenum > linenum) {
        program[i] = program[i - 1];
        i--;
    }
    int len = strlen(text);
    program[i].text = basic_alloc(len + 1);
    if (!program[i].text) {
        os_print("?OUT OF MEMORY\n");
        return;
    }
    program[i].linenum = linenum;
    strcpy(program[i].text, text);
    program_count++;
}

/* ---- DOS Menu ---- */

static void dos_menu(void) {
    int save_fg, save_bg;
    os_get_color(&save_fg, &save_bg);
    for (;;) {
        os_set_color(OS_YELLOW, OS_BLACK);
        os_print("\n");
        os_print(" ================================\n");
        os_print("  ICARUS DOS\n");
        os_print(" ================================\n");
        os_set_color(OS_LCYAN, OS_BLACK);
        os_print("  D - Directory\n");
        os_print("  L - Load file\n");
        os_print("  S - Save program\n");
        os_print("  E - Erase file\n");
        os_print("  F - Format disk\n");
        os_print("  B - Back to BASIC\n");
        os_print("\n");
        os_print("  Choice: ");

        char c = 0;
        while (!c) c = os_read_key();
        os_putchar(c);
        os_putchar('\n');

        if (c == 'b' || c == 'B') {
            os_set_color(save_fg, save_bg);
            return;
        }

        if (c == 'd' || c == 'D') {
            os_print("\n");
            os_list_files();
        } else if (c == 'f' || c == 'F') {
            os_print(" Are you sure? (Y/N) ");
            char yn = 0;
            while (!yn) yn = os_read_key();
            os_putchar(yn);
            os_putchar('\n');
            if (yn == 'y' || yn == 'Y') {
                if (os_format_disk() == 0)
                    os_print(" Formatted.\n");
                else
                    os_print(" ?FORMAT ERROR\n");
            }
        } else if (c == 's' || c == 'S') {
            os_print(" Filename: ");
            char name[BASIC_FILENAME_LEN];
            if (read_line(name, BASIC_FILENAME_LEN) > 0) {
                static char savebuf[16384];
                int size = basic_program_serialize(savebuf, sizeof(savebuf));
                if (size > 0) {
                    if (os_save_file(name, savebuf, size) == 0)
                        os_print(" Saved.\n");
                } else {
                    os_print(" ?NO PROGRAM\n");
                }
            }
        } else if (c == 'l' || c == 'L') {
            os_print(" Filename: ");
            char name[BASIC_FILENAME_LEN];
            if (read_line(name, BASIC_FILENAME_LEN) > 0) {
                static char loadbuf[16384];
                int size = os_load_file(name, loadbuf, sizeof(loadbuf));
                if (size > 0) {
                    basic_program_deserialize(loadbuf, size);
                    os_print(" Loaded.\n");
                }
            }
        } else if (c == 'e' || c == 'E') {
            os_print(" Filename: ");
            char name[BASIC_FILENAME_LEN];
            if (read_line(name, BASIC_FILENAME_LEN) > 0) {
                if (os_delete_file(name) == 0)
                    os_print(" Deleted.\n");
            }
        }
    }
}

/* ---- Init ---- */

void basic_init(void) {
    basic_alloc_reset();
    program_count = 0;
    for_depth = 0;
    gosub_depth = 0;
    num_var_count = 0;
    str_var_count = 0;
    array_count = 0;
}

/* ---- Public entry point ---- */

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

    /* CLR command -- clear program */
    if (tokens[0].type == TOK_CLR) {
        basic_init();
        os_print("OK\n");
        return;
    }

    /* QUIT command -- shutdown */
    if (tokens[0].type == TOK_QUIT) {
        os_print("SHUTTING DOWN...\n");
        os_shutdown();
        return;
    }

    /* SAVE "filename" */
    if (tokens[0].type == TOK_SAVE) {
        if (tokens[1].type != TOK_STRING) {
            os_print("?SYNTAX ERROR\n");
            return;
        }
        static char savebuf[16384];
        int size = basic_program_serialize(savebuf, sizeof(savebuf));
        if (size <= 0) {
            os_print("?NO PROGRAM\n");
            return;
        }
        if (os_save_file(tokens[1].string_val, savebuf, size) == 0)
            os_print("SAVED\n");
        return;
    }

    /* LOAD "filename" */
    if (tokens[0].type == TOK_LOAD) {
        if (tokens[1].type != TOK_STRING) {
            os_print("?SYNTAX ERROR\n");
            return;
        }
        static char loadbuf[16384];
        int size = os_load_file(tokens[1].string_val, loadbuf, sizeof(loadbuf));
        if (size > 0) {
            basic_program_deserialize(loadbuf, size);
            os_print("LOADED\n");
        }
        return;
    }

    /* DIR */
    if (tokens[0].type == TOK_DIR) {
        os_list_files();
        return;
    }

    /* DELETE "filename" */
    if (tokens[0].type == TOK_DELETE) {
        if (tokens[1].type != TOK_STRING) {
            os_print("?SYNTAX ERROR\n");
            return;
        }
        if (os_delete_file(tokens[1].string_val) == 0)
            os_print("DELETED\n");
        return;
    }

    /* FORMAT */
    if (tokens[0].type == TOK_FORMAT) {
        os_print("FORMAT DISK - ARE YOU SURE? (Y/N) ");
        char c = 0;
        while (c != 'y' && c != 'Y' && c != 'n' && c != 'N')
            c = os_read_key();
        os_putchar(c);
        os_putchar('\n');
        if (c == 'y' || c == 'Y') {
            if (os_format_disk() == 0)
                os_print("FORMATTED\n");
            else
                os_print("?FORMAT ERROR\n");
        }
        return;
    }

    /* DOS -- Atari-style disk utility menu */
    if (tokens[0].type == TOK_DOS) {
        dos_menu();
        return;
    }

    /* If the line starts with a number, store it as a program line */
    if (tokens[0].type == TOK_NUMBER) {
        int linenum = (int)tokens[0].number_val;
        /* Find where the line number ends in the input */
        const char *p = line;
        while (*p == ' ') p++;
        while (*p >= '0' && *p <= '9') p++;
        while (*p == ' ') p++;
        if (*p == '\0') {
            /* Just a line number -- delete the line */
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
