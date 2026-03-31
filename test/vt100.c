/*
 * Tests for the VT100 escape sequence interpreter.
 * Uses mock OS functions to capture what the interpreter does.
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>

/* --- Mock OS state --- */

static int mock_cursor_row;
static int mock_cursor_col;
static int mock_fg;
static int mock_bg;
static int mock_cursor_visible;
static char mock_screen[25][80];
static int mock_screen_rows = 25;
static int mock_screen_cols = 80;

/* Color constants (matching os.h) */
#define OS_BLACK        0
#define OS_BLUE         1
#define OS_GREEN        2
#define OS_CYAN         3
#define OS_RED          4
#define OS_MAGENTA      5
#define OS_BROWN        6
#define OS_LGRAY        7
#define OS_DGRAY        8
#define OS_LBLUE        9
#define OS_LGREEN      10
#define OS_LCYAN       11
#define OS_LRED        12
#define OS_LMAGENTA    13
#define OS_YELLOW      14
#define OS_WHITE       15

/* --- Mock OS functions (called by vt100.c) --- */

void os_putchar(char c) {
    if (c == '\n') {
        mock_cursor_col = 0;
        if (mock_cursor_row < mock_screen_rows - 1)
            mock_cursor_row++;
    } else if (c == '\r') {
        mock_cursor_col = 0;
    } else {
        if (mock_cursor_row < mock_screen_rows && mock_cursor_col < mock_screen_cols) {
            mock_screen[mock_cursor_row][mock_cursor_col] = c;
            mock_cursor_col++;
        }
    }
}

void os_set_color(int fg, int bg) { mock_fg = fg; mock_bg = bg; }
void os_get_color(int *fg, int *bg) { *fg = mock_fg; *bg = mock_bg; }
void os_clear_screen(void) {
    memset(mock_screen, ' ', sizeof(mock_screen));
    mock_cursor_row = 0;
    mock_cursor_col = 0;
}
void os_set_cursor(int row, int col) {
    if (row < 0) row = 0;
    if (col < 0) col = 0;
    if (row >= mock_screen_rows) row = mock_screen_rows - 1;
    if (col >= mock_screen_cols) col = mock_screen_cols - 1;
    mock_cursor_row = row;
    mock_cursor_col = col;
}
void os_clear_to_eol(void) {
    for (int c = mock_cursor_col; c < mock_screen_cols; c++)
        mock_screen[mock_cursor_row][c] = ' ';
}
void os_show_cursor(int show) { mock_cursor_visible = show; }
size_t os_cursor_row(void) { return mock_cursor_row; }
size_t os_cursor_col(void) { return mock_cursor_col; }
size_t os_screen_rows(void) { return mock_screen_rows; }
size_t os_screen_cols(void) { return mock_screen_cols; }

/* --- Include vt100.c directly (it only depends on the os_ functions above) --- */

/* Prevent it from including the real headers */
#define VGA_H
#define OS_H
#include "../os/vt100.c"

/* --- Helper --- */
static void mock_reset(void) {
    memset(mock_screen, ' ', sizeof(mock_screen));
    mock_cursor_row = 0;
    mock_cursor_col = 0;
    mock_fg = OS_LGRAY;
    mock_bg = OS_BLACK;
    mock_cursor_visible = 1;
    state = VT100_NORMAL;
    nparams = 0;
}

/* --- Tests --- */

int main(void) {
    /* 1. Plain text goes to screen */
    mock_reset();
    vt100_write("Hi", 2);
    assert(mock_screen[0][0] == 'H');
    assert(mock_screen[0][1] == 'i');
    assert(mock_cursor_col == 2);
    printf("  1. plain text = OK\n");

    /* 2. Cursor home \x1b[H */
    mock_reset();
    mock_cursor_row = 5;
    mock_cursor_col = 10;
    vt100_write("\x1b[H", 3);
    assert(mock_cursor_row == 0);
    assert(mock_cursor_col == 0);
    printf("  2. cursor home = OK\n");

    /* 3. Cursor position \x1b[row;colH (1-indexed) */
    mock_reset();
    vt100_write("\x1b[3;5H", 6);
    assert(mock_cursor_row == 2);  /* 3-1 */
    assert(mock_cursor_col == 4);  /* 5-1 */
    printf("  3. cursor position = OK\n");

    /* 4. Clear to end of line \x1b[0K */
    mock_reset();
    vt100_write("ABCDEF", 6);
    vt100_write("\x1b[1;3H", 6);  /* position at row 1, col 3 (0-indexed: 0,2) */
    vt100_write("\x1b[0K", 4);
    assert(mock_screen[0][0] == 'A');
    assert(mock_screen[0][1] == 'B');
    assert(mock_screen[0][2] == ' ');  /* cleared */
    assert(mock_screen[0][3] == ' ');  /* cleared */
    printf("  4. clear to EOL = OK\n");

    /* 5. Set foreground color \x1b[31m (red) */
    mock_reset();
    vt100_write("\x1b[31m", 5);
    assert(mock_fg == OS_RED);
    assert(mock_bg == OS_BLACK);  /* unchanged */
    printf("  5. set foreground red = OK\n");

    /* 6. Reset attributes \x1b[0m */
    mock_reset();
    vt100_write("\x1b[31m", 5);   /* set red via interpreter */
    assert(mock_fg == OS_RED);
    vt100_write("\x1b[0m", 4);
    assert(mock_fg == OS_LGRAY);
    assert(mock_bg == OS_BLACK);
    printf("  6. reset attributes = OK\n");

    /* 7. Reverse video \x1b[7m */
    mock_reset();
    mock_fg = OS_LGRAY;
    mock_bg = OS_BLACK;
    vt100_write("\x1b[7m", 4);
    assert(mock_fg == OS_BLACK);
    assert(mock_bg == OS_LGRAY);
    printf("  7. reverse video = OK\n");

    /* 8. Reset foreground \x1b[39m */
    mock_reset();
    vt100_write("\x1b[31m", 5);   /* set red via interpreter */
    assert(mock_fg == OS_RED);
    vt100_write("\x1b[39m", 5);
    assert(mock_fg == OS_LGRAY);  /* back to default */
    assert(mock_bg == OS_BLACK);  /* unchanged */
    printf("  8. reset foreground = OK\n");

    /* 9. Hide cursor \x1b[?25l */
    mock_reset();
    vt100_write("\x1b[?25l", 6);
    assert(mock_cursor_visible == 0);
    printf("  9. hide cursor = OK\n");

    /* 10. Show cursor \x1b[?25h */
    mock_reset();
    mock_cursor_visible = 0;
    vt100_write("\x1b[?25h", 6);
    assert(mock_cursor_visible == 1);
    printf(" 10. show cursor = OK\n");

    /* 11. Multiple colors in sequence */
    mock_reset();
    vt100_write("\x1b[32m", 5);  /* green */
    assert(mock_fg == OS_GREEN);
    vt100_write("\x1b[34m", 5);  /* blue */
    assert(mock_fg == OS_BLUE);
    vt100_write("\x1b[0m", 4);   /* reset */
    assert(mock_fg == OS_LGRAY);
    printf(" 11. color sequence = OK\n");

    /* 12. Mixed text and escapes */
    mock_reset();
    vt100_write("\x1b[HHello\x1b[2;1HWorld", 19);
    assert(mock_screen[0][0] == 'H');
    assert(mock_screen[0][4] == 'o');
    assert(mock_screen[1][0] == 'W');
    assert(mock_screen[1][4] == 'd');
    printf(" 12. mixed text + escapes = OK\n");

    /* 13. \r\n moves to start of next line */
    mock_reset();
    vt100_write("AB\r\nCD", 6);
    assert(mock_screen[0][0] == 'A');
    assert(mock_screen[0][1] == 'B');
    assert(mock_screen[1][0] == 'C');
    assert(mock_screen[1][1] == 'D');
    printf(" 13. CR+LF = OK\n");

    /* 14. Cursor movement: down and right */
    mock_reset();
    vt100_write("\x1b[5B", 4);   /* down 5 */
    assert(mock_cursor_row == 5);
    vt100_write("\x1b[10C", 5);  /* right 10 */
    assert(mock_cursor_col == 10);
    printf(" 14. cursor down+right = OK\n");

    /* 15. Bright/high-intensity colors */
    mock_reset();
    vt100_write("\x1b[91m", 5);  /* bright red */
    assert(mock_fg == OS_LRED);
    vt100_write("\x1b[97m", 5);  /* bright white */
    assert(mock_fg == OS_WHITE);
    printf(" 15. bright colors = OK\n");

    /* 16. Unknown escape sequence doesn't crash */
    mock_reset();
    vt100_write("\x1b[999Z", 6);  /* unknown final char */
    vt100_write("OK", 2);         /* should still work after */
    assert(mock_screen[0][0] == 'O');
    assert(mock_screen[0][1] == 'K');
    printf(" 16. unknown escape ignored = OK\n");

    /* 17. Partial escape across writes */
    mock_reset();
    vt100_write("\x1b", 1);      /* just ESC */
    vt100_write("[", 1);          /* just [ */
    vt100_write("3;5H", 4);      /* params + final */
    assert(mock_cursor_row == 2);
    assert(mock_cursor_col == 4);
    printf(" 17. split escape across writes = OK\n");

    /* 18. Erase display \x1b[2J */
    mock_reset();
    vt100_write("ABCDEF", 6);
    vt100_write("\x1b[2J", 4);
    assert(mock_screen[0][0] == ' ');
    assert(mock_screen[0][5] == ' ');
    assert(mock_cursor_row == 0);
    assert(mock_cursor_col == 0);
    printf(" 18. erase display = OK\n");

    /* 19. Bare \x1b[K defaults to clear-to-EOL (same as \x1b[0K) */
    mock_reset();
    vt100_write("ABCDEF", 6);
    vt100_write("\x1b[1;3H", 6);  /* col 2 */
    vt100_write("\x1b[K", 3);     /* no parameter */
    assert(mock_screen[0][0] == 'A');
    assert(mock_screen[0][1] == 'B');
    assert(mock_screen[0][2] == ' ');
    assert(mock_screen[0][3] == ' ');
    printf(" 19. bare ESC[K defaults = OK\n");

    printf("\nAll tests passed.\n");
    return 0;
}
