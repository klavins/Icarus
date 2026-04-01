# Writing C Programs for ICARUS

ICARUS can run C programs alongside BASIC. Programs are written in standard C99 with a `main` function and access OS services through header files in the `include/` directory.

## Hello World

```c
#include "os.h"

int main(void) {
    os_print("Hello, ICARUS!\n");
    os_read_key();
    return 0;
}
```

When `main` returns, control passes back to the BASIC prompt.

## Headers

Include only what your program needs:

| Header | Contents |
|---|---|
| `os.h` | Display, input, sound, disk, timer |
| `graphics.h` | Pixel graphics (plot, drawto, fillto, text) |
| `math.h` | sin, cos, sqrt, PI |
| `string.h` | strlen, strcmp, memcpy, snprintf, etc. |
| `malloc.h` | malloc, free, realloc, calloc |

Or include `icarus.h` to get everything at once.

## Display

```c
#include "os.h"

os_clear_screen();
os_set_color(OS_GREEN, OS_BLACK);
os_print("Green text\n");
os_putchar('!');
```

Colors: `OS_BLACK`, `OS_BLUE`, `OS_GREEN`, `OS_CYAN`, `OS_RED`, `OS_MAGENTA`, `OS_BROWN`, `OS_LGRAY`, `OS_DGRAY`, `OS_LBLUE`, `OS_LGREEN`, `OS_LCYAN`, `OS_LRED`, `OS_LMAGENTA`, `OS_YELLOW`, `OS_WHITE`.

### VT100 Output

`os_write` sends raw bytes through the VT100 terminal emulator, supporting escape sequences for cursor positioning, colors, and screen clearing:

```c
os_write("\x1b[2J", 4);           /* clear screen */
os_write("\x1b[5;10H", 7);        /* move cursor to row 5, col 10 */
os_write("\x1b[31mRed\x1b[0m", 14); /* red text, then reset */
```

### Cursor and Screen

```c
os_set_cursor(row, col);    /* 0-indexed */
os_clear_to_eol();
os_show_cursor(0);           /* hide */
os_show_cursor(1);           /* show */
int cols = os_screen_cols();
int rows = os_screen_rows();
```

## Input

```c
#include "os.h"

char c = os_read_key();      /* blocks until keypress, returns ASCII */
int  k = os_read_key_ext();  /* returns ASCII or special key code (128+) */
int held = os_key_state(0x4D); /* 1 if right arrow is currently held */
os_flush_keys();
```

Special key codes from `os_read_key_ext`: `OS_KEY_ARROW_UP` (128), `OS_KEY_ARROW_DOWN` (129), `OS_KEY_ARROW_LEFT` (130), `OS_KEY_ARROW_RIGHT` (131), `OS_KEY_HOME` (132), `OS_KEY_END` (133), `OS_KEY_PAGE_UP` (134), `OS_KEY_PAGE_DOWN` (135), `OS_KEY_DELETE` (136).

## Graphics

```c
#include "os.h"
#include "graphics.h"

gfx_set_mode(2);          /* 320x200 chunky pixels */
gfx_set_color(10);        /* light green */
gfx_plot(100, 50);        /* move pen and draw a pixel */
gfx_drawto(200, 150);     /* draw line from current position */
gfx_fillto(200, 150);     /* draw filled rectangle from current position */
gfx_pixel(10, 10, 14);    /* set a single pixel to yellow */
gfx_pos(50, 80);          /* move pen without drawing */
gfx_text("HELLO");        /* draw text at pen position */
gfx_present();             /* flush to screen */
gfx_set_mode(0);          /* return to text mode */
```

Graphics modes:

| Mode | Resolution | Description |
|---|---|---|
| 0 | text | Text mode, 1x font |
| 1 | text | Text mode, 2x font |
| 2 | ~320px wide | Chunky pixels (Atari-style) |
| 3 | ~640px wide | Medium resolution |
| 4 | ~800px wide | High resolution |
| 5 | native | Full display resolution |

All pixel modes use 32-bit color internally, with a 16-color palette matching the text mode colors (0-15).

## Math

```c
#include "math.h"

double s = sin(1.0);
double c = cos(PI / 4);
double r = sqrt(2.0);
```

## Strings and Memory

```c
#include "string.h"
#include "malloc.h"

char *s = strdup("hello");
int len = strlen(s);
char buf[64];
snprintf(buf, sizeof(buf), "length = %d\n", len);
free(s);

void *p = malloc(1024);
memset(p, 0, 1024);
free(p);
```

Available string functions: `strlen`, `strcmp`, `strncmp`, `strcpy`, `strncpy`, `strdup`, `strchr`, `strstr`, `memset`, `memcpy`, `memmove`, `memcmp`, `snprintf`.

## Sound

```c
#include "os.h"

os_tone(100);       /* play tone (pitch 0-255, higher = lower frequency) */
os_delay_ms(500);   /* wait 500ms */
os_tone_off();      /* silence */
```

## Disk

```c
#include "os.h"

/* Save data */
char data[] = "file contents";
os_save_file("myfile.txt", data, strlen(data));

/* Load data */
char buf[4096];
int size = os_load_file("myfile.txt", buf, sizeof(buf));

/* Other operations */
os_delete_file("myfile.txt");
int exists = os_file_exists("myfile.txt");
os_list_files();   /* prints file list to screen */
```

File names are limited to 31 characters.

## Timer

```c
#include "os.h"

os_delay_ms(1000);           /* sleep 1 second */
uint32_t t = os_ticks();     /* system tick count (200 Hz) */
```

## Example: Graphics Demo

See `examples/demo.c` for a complete program that draws borders, circles, and text in each graphics mode.
