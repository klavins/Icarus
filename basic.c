#include "basic_internal.h"

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

/* ---- Init ---- */

void basic_init(void) {
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
        terminal_print("OK\n");
        return;
    }

    /* QUIT command -- shutdown */
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
