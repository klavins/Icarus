/*
 * vt100.c - VT100/ANSI escape sequence interpreter
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

#include "vga.h"
#include "os.h"

/*
 * State machine for parsing ANSI escape sequences.
 *
 * Normal: characters go to os_putchar.
 * On ESC (\x1b): enter ESC state.
 * On '[' after ESC: enter CSI state, collect params.
 * On final letter: execute the command.
 */

#define VT100_NORMAL 0
#define VT100_ESC    1
#define VT100_CSI    2
#define VT100_QMARK  3   /* CSI with '?' prefix, e.g. \x1b[?25h */

#define MAX_PARAMS 4

static int state;
static int params[MAX_PARAMS];
static int nparams;

/* Map ANSI SGR color (30-37) to OS color constants */
static int ansi_to_os_color(int ansi) {
    switch (ansi) {
    case 30: return OS_BLACK;
    case 31: return OS_RED;
    case 32: return OS_GREEN;
    case 33: return OS_BROWN;
    case 34: return OS_BLUE;
    case 35: return OS_MAGENTA;
    case 36: return OS_CYAN;
    case 37: return OS_LGRAY;
    case 90: return OS_DGRAY;
    case 91: return OS_LRED;
    case 92: return OS_LGREEN;
    case 93: return OS_YELLOW;
    case 94: return OS_LBLUE;
    case 95: return OS_LMAGENTA;
    case 96: return OS_LCYAN;
    case 97: return OS_WHITE;
    default: return OS_LGRAY;
    }
}

static int default_fg = OS_LGRAY;
static int default_bg = OS_BLACK;

/* Execute a CSI sequence ending with the given final character */
static void csi_dispatch(char final) {
    switch (final) {
    case 'H': /* Cursor position: \x1b[row;colH */
    case 'f': {
        int row = (nparams > 0 && params[0] > 0) ? params[0] - 1 : 0;
        int col = (nparams > 1 && params[1] > 0) ? params[1] - 1 : 0;
        os_set_cursor(row, col);
        break;
    }
    case 'K': { /* Erase in line */
        int mode = (nparams > 0) ? params[0] : 0;
        if (mode == 0) os_clear_to_eol();
        /* mode 1 (clear to start) and mode 2 (clear whole line) not needed */
        break;
    }
    case 'm': { /* SGR — Set Graphics Rendition */
        if (nparams == 0) {
            /* \x1b[m with no params = reset */
            os_set_color(default_fg, default_bg);
            break;
        }
        for (int i = 0; i < nparams; i++) {
            int p = params[i];
            if (p == 0) {
                /* Reset */
                os_set_color(default_fg, default_bg);
            } else if (p == 7) {
                /* Reverse video */
                int fg, bg;
                os_get_color(&fg, &bg);
                os_set_color(bg, fg);
            } else if (p == 39) {
                /* Default foreground */
                int fg, bg;
                os_get_color(&fg, &bg);
                os_set_color(default_fg, bg);
            } else if (p == 49) {
                /* Default background */
                int fg, bg;
                os_get_color(&fg, &bg);
                os_set_color(fg, default_bg);
            } else if ((p >= 30 && p <= 37) || (p >= 90 && p <= 97)) {
                /* Foreground color */
                int fg, bg;
                os_get_color(&fg, &bg);
                os_set_color(ansi_to_os_color(p), bg);
            } else if ((p >= 40 && p <= 47) || (p >= 100 && p <= 107)) {
                /* Background color */
                int fg, bg;
                os_get_color(&fg, &bg);
                os_set_color(fg, ansi_to_os_color(p - 10));
            }
        }
        break;
    }
    case 'A': { /* Cursor up */
        int n = (nparams > 0 && params[0] > 0) ? params[0] : 1;
        int row = (int)os_cursor_row() - n;
        if (row < 0) row = 0;
        os_set_cursor(row, (int)os_cursor_col());
        break;
    }
    case 'B': { /* Cursor down */
        int n = (nparams > 0 && params[0] > 0) ? params[0] : 1;
        int row = (int)os_cursor_row() + n;
        os_set_cursor(row, (int)os_cursor_col());
        break;
    }
    case 'C': { /* Cursor forward (right) */
        int n = (nparams > 0 && params[0] > 0) ? params[0] : 1;
        int col = (int)os_cursor_col() + n;
        os_set_cursor((int)os_cursor_row(), col);
        break;
    }
    case 'D': { /* Cursor backward (left) */
        int n = (nparams > 0 && params[0] > 0) ? params[0] : 1;
        int col = (int)os_cursor_col() - n;
        if (col < 0) col = 0;
        os_set_cursor((int)os_cursor_row(), col);
        break;
    }
    case 'J': { /* Erase in display */
        int mode = (nparams > 0) ? params[0] : 0;
        if (mode == 2) os_clear_screen();
        break;
    }
    default:
        break; /* Unknown sequence — ignore */
    }
}

/* Execute a CSI ? sequence (private mode) */
static void csi_qmark_dispatch(char final) {
    if (nparams > 0 && params[0] == 25) {
        if (final == 'h') os_show_cursor(1);      /* ?25h — show cursor */
        else if (final == 'l') os_show_cursor(0);  /* ?25l — hide cursor */
    }
}

static void vt100_process(char c) {
    switch (state) {
    case VT100_NORMAL:
        if (c == '\x1b') {
            state = VT100_ESC;
        } else {
            os_putchar(c);
        }
        break;

    case VT100_ESC:
        if (c == '[') {
            state = VT100_CSI;
            nparams = 0;
            for (int i = 0; i < MAX_PARAMS; i++) params[i] = 0;
        } else {
            /* Unknown ESC sequence — ignore */
            state = VT100_NORMAL;
        }
        break;

    case VT100_CSI:
        if (c == '?') {
            state = VT100_QMARK;
        } else if (c >= '0' && c <= '9') {
            if (nparams == 0) nparams = 1;
            params[nparams - 1] = params[nparams - 1] * 10 + (c - '0');
        } else if (c == ';') {
            if (nparams < MAX_PARAMS) nparams++;
        } else {
            /* Final character — execute */
            csi_dispatch(c);
            state = VT100_NORMAL;
        }
        break;

    case VT100_QMARK:
        if (c >= '0' && c <= '9') {
            if (nparams == 0) nparams = 1;
            params[nparams - 1] = params[nparams - 1] * 10 + (c - '0');
        } else if (c == ';') {
            if (nparams < MAX_PARAMS) nparams++;
        } else {
            csi_qmark_dispatch(c);
            state = VT100_NORMAL;
        }
        break;
    }
}

void vt100_write(const char *buf, int len) {
    terminal_flush_lock();
    for (int i = 0; i < len; i++)
        vt100_process(buf[i]);
    terminal_flush_unlock();
}
