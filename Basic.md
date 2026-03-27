# ICARUS BASIC Reference

ICARUS BASIC is a BASIC interpreter running on the ICARUS OS, a bare-metal x86 kernel. It boots on both legacy BIOS and modern 64-bit UEFI systems. Inspired by Atari BASIC.

## Getting Started

Build and run in QEMU:

    make sim-uefi64

The system boots, displays hardware info, and drops into the BASIC prompt:

    ICARUS BASIC
     >

Commands can be entered directly (immediate mode) or stored as numbered lines (program mode).

## Variables

- Numeric variables: multi-character names (e.g., X, SCORE, PLAYER1). Case-insensitive.
- String variables: end with $ (e.g., N$, NAME$). Must be DIMmed before use.
- Arrays: A(n) or A(r,c) for 2D. Must be DIMmed before use.

## Numbers

All numbers are double-precision floating point. Integer and decimal literals are supported:

    PRINT 42
    PRINT 3.14
    PRINT 0xFF

Hex literals use the `0x` prefix. They are stored as doubles like all numbers.

## Expressions

Arithmetic: `+`, `-`, `*`, `/`, `MOD`, `(`, `)`

    PRINT 2 + 3 * (4 - 1)
    PRINT 1 / 3
    PRINT 3.14 * 2
    PRINT 10 MOD 3

`MOD` returns the integer remainder. It has the same precedence as `*` and `/`. Operands are truncated to integers before the operation.

Comparisons (used with IF): `=`, `<`, `>`, `<=`, `>=`, `<>`

### Built-in Functions

    RND(N)     Random integer from 0 to N-1 (N > 1)
    RND        Random float from 0.0 to ~1.0 (no argument)
    ABS(N)     Absolute value
    INT(N)     Truncate to integer (INT(3.7) = 3)
    SQR(N)     Square root
    SIN(N)     Sine (radians)
    COS(N)     Cosine (radians)
    LEN(A$)    Length of a string
    PEEK(addr) Read a byte from memory

### Built-in Values

    PI         3.14159265358979...
    SCRW       Screen width in pixels
    SCRH       Screen height in pixels

## Commands

### PRINT

Print values separated by commas. Supports numbers, strings, variables, and expressions.

    PRINT "HELLO WORLD"
    PRINT 2 + 2
    PRINT "X = ", X
    PRINT 1, "TWO", 3

### LET

Assign a value to a variable.

    LET X = 10
    LET SCORE = X * 2 + 1
    LET A$ = "HELLO"
    LET A(5) = 42
    LET M(1,2) = 99

### DIM

Declare a string variable or array with a given size. Memory is allocated dynamically from the heap — arrays can be any size that fits in available memory.

    DIM A$(20)
    DIM A(10)
    DIM A(100000)
    DIM M(3,3)

String variables must be DIMmed before assignment. Arrays are 0-based. If memory is exhausted, prints `?OUT OF MEMORY`. Use `CLR` to free all allocated memory.

### IF...THEN

Conditional execution. The part after THEN can be any command.

    IF X > 5 THEN PRINT "BIG"
    IF X = 0 THEN LET X = 1
    IF X <> Y THEN GOTO 100

### FOR...NEXT

Counted loop. STEP is optional (defaults to 1).

    FOR I = 1 TO 10
    FOR I = 10 TO 0 STEP -2

Must be paired with NEXT:

    10 FOR I = 1 TO 5
    20 PRINT I
    30 NEXT I

### GOTO

Jump to a line number.

    10 PRINT "LOOP"
    20 GOTO 10

### GOSUB / RETURN

Call a subroutine and return from it.

    10 GOSUB 100
    20 PRINT "BACK"
    30 GOTO 999
    100 PRINT "IN SUB"
    110 RETURN
    999 PRINT "DONE"

### ON...GOTO / ON...GOSUB

Branch to one of several line numbers based on a value (1-based).

    ON X GOTO 100, 200, 300
    ON X GOSUB 100, 200, 300

If the value is out of range, execution falls through to the next line.

### READ / DATA / RESTORE

Read values from DATA statements.

    10 DATA 10, 20, 30
    20 DATA "ALICE", "BOB"
    30 READ X
    40 READ A$
    50 RESTORE

DATA values are collected from all DATA lines in order before RUN. RESTORE resets the read pointer to the beginning.

### INPUT

Read user input into a variable. Supports an optional prompt string.

    INPUT X
    INPUT "YOUR NAME? ", N$
    INPUT "AGE? ", A

### REM

Comment. Everything after REM is ignored.

    10 REM This is a comment
    20 REM ==================
    30 PRINT "HELLO"

### COLOR

Set the text color (in text mode) or drawing color (in graphics mode).

Text mode uses VGA color codes 0-15:

    0  Black       8  Dark Gray
    1  Blue        9  Light Blue
    2  Green      10  Light Green
    3  Cyan       11  Light Cyan
    4  Red        12  Light Red
    5  Magenta    13  Light Magenta
    6  Brown      14  Yellow
    7  Light Gray 15  White

Graphics mode uses the standard VGA 256-color palette (0-255). The first 16 match the text mode colors. Colors are automatically mapped to 32-bit RGB on UEFI displays.

    COLOR 14
    PRINT "THIS IS YELLOW"

### SOUND

Play a tone through the PC speaker. Syntax matches Atari BASIC:

    SOUND voice, pitch, distortion, volume

- voice: ignored (PC speaker has one channel)
- pitch: 0-255 (higher value = lower frequency)
- distortion: ignored (PC speaker only does square waves)
- volume: 0 = off, any other value = on

Examples:

    SOUND 0, 100, 0, 8
    SOUND 0, 0, 0, 0

### GRAPHICS

Switch video mode. Mode 0 returns to text mode and restores the previous screen contents. Drawing uses double buffering — all pixels go to a shadow buffer and are presented to the display automatically.

    GRAPHICS 0
    GRAPHICS 1
    GRAPHICS 2
    GRAPHICS 3
    GRAPHICS 4

- 0 = text mode
- 1 = 320x200 (chunky pixels, Atari-style)
- 2 = 640x400 (medium resolution)
- 3 = 800x600 (high resolution)
- 4 = native resolution (whatever the display supports)

In UEFI mode, lower resolution modes scale up to fill the screen. In VGA mode, only mode 1 is available as a true hardware mode.

### PLOT

Set a pixel and move the graphics cursor (graphics mode only).

    PLOT 160, 100

### DRAWTO

Draw a line from the current graphics cursor to the given point.

    PLOT 10, 10
    DRAWTO 100, 10
    DRAWTO 100, 100
    DRAWTO 10, 100
    DRAWTO 10, 10

### POS

Set the graphics cursor position without drawing.

    POS 100, 50

### TEXT

Draw text at the current graphics cursor position using the bitmap font. Uses the current COLOR. Works with string literals, string variables, and numeric expressions.

    POS 10, 10
    TEXT "HELLO WORLD"
    POS 10, 30
    TEXT N$
    POS 10, 50
    TEXT X * 2

Each character is 8 pixels wide and 16 pixels tall. The cursor advances by 8 pixels after each character.

### SCRW / SCRH

Built-in values that return the current screen width and height in pixels. In text mode, returns the physical display resolution. In graphics mode, returns the virtual resolution of the current mode.

    PRINT SCRW, SCRH
    POS SCRW / 2 - 20, SCRH / 2
    TEXT "CENTER"

### DELAY

Pause execution for a given number of milliseconds. In graphics mode, automatically presents the shadow buffer to the display before waiting.

    DELAY 16
    DELAY 1000

For game loops, `DELAY 14` gives approximately 70 frames per second.

### PAUSE

Flush the display and wait for any keypress before continuing.

    PAUSE

### PEEK / POKE

Read or write a byte at a memory address.

    LET V = PEEK(458881)
    POKE 458753, 0

### System Status Area

The interrupt handler maintains a system status area at address 458752 (0x70000):

    458752 - 458879    Key state (128 bytes, 1=pressed, 0=released)
                       Address = 458752 + scancode
    458880             Last key scancode
    458881             Last key ASCII value

Common scancodes:

    ESC=1  W=17  A=30  S=31  D=32  SPACE=57
    UP=72  DOWN=80  LEFT=75  RIGHT=77

Example — real-time key checking in a game loop:

    100 IF PEEK(458769) = 1 THEN LET Y = Y - 1
    110 IF PEEK(458783) = 1 THEN LET Y = Y + 1
    120 IF PEEK(458782) = 1 THEN LET X = X - 1
    130 IF PEEK(458784) = 1 THEN LET X = X + 1

## Program Management

### Line Numbers

Lines starting with a number are stored as a program. Enter a line number alone to delete that line.

    10 PRINT "HELLO"
    20 GOTO 10
    20

The last command deletes line 20.

### RUN

Execute the stored program.

    RUN

Press Escape to break out of a running program. If in graphics mode, the display is automatically restored to text mode.

### LIST

Display the stored program.

    LIST

### CLR

Clear the stored program, all variables, and free all dynamically allocated memory (arrays, strings, program text).

    CLR

### QUIT

Shut down the system (closes QEMU).

    QUIT

## Disk Commands

The disk must be formatted before first use.

### FORMAT

Format the virtual disk. Erases all files.

    FORMAT

### SAVE

Save the current program to disk.

    SAVE "MYFILE"

### LOAD

Load a program from disk (replaces current program).

    LOAD "MYFILE"

### DIR

List files on disk and show free space.

    DIR

### DELETE

Delete a file from disk.

    DELETE "MYFILE"

### DOS

Enter the Atari-style disk utility menu with options for Directory, Load, Save, Erase, Format, and Back to BASIC.

    DOS

## Example Programs

### Hello World

    10 PRINT "HELLO WORLD"
    RUN

### Number Guessing Game

    10 LET N = RND(100) + 1
    20 INPUT "GUESS? ", G
    30 IF G = N THEN GOTO 60
    40 IF G < N THEN PRINT "TOO LOW"
    50 IF G > N THEN PRINT "TOO HIGH"
    55 GOTO 20
    60 PRINT "YOU GOT IT!"
    RUN

### Graphics with Text

    10 GRAPHICS 1
    20 COLOR 14
    30 POS SCRW / 2 - 40, SCRH / 2
    40 TEXT "HELLO WORLD"
    50 PAUSE
    60 GRAPHICS 0
    RUN

### Real-time Input (Game Loop)

    10 POKE 458809, 0
    20 GRAPHICS 1
    30 LET X = 160
    40 LET Y = 100
    50 COLOR 15
    60 PLOT X, Y
    100 IF PEEK(458769) = 1 THEN LET Y = Y - 1
    110 IF PEEK(458783) = 1 THEN LET Y = Y + 1
    120 IF PEEK(458782) = 1 THEN LET X = X - 1
    130 IF PEEK(458784) = 1 THEN LET X = X + 1
    140 PLOT X, Y
    150 DELAY 14
    160 IF PEEK(458809) = 1 THEN GOTO 200
    170 GOTO 100
    200 GRAPHICS 0
    RUN

### Subroutines

    10 LET X = 5
    20 GOSUB 100
    30 LET X = 10
    40 GOSUB 100
    50 GOTO 999
    100 PRINT "SQUARE OF ", X, " IS ", X * X
    110 RETURN
    999 PRINT "DONE"
    RUN

### Floating Point Math

    10 PRINT "PI = ", PI
    20 PRINT "SIN(PI/4) = ", SIN(PI / 4)
    30 PRINT "COS(PI/4) = ", COS(PI / 4)
    40 PRINT "SQR(2) = ", SQR(2)
    50 FOR X = 0 TO 1 STEP 0.1
    60 PRINT X, " "
    70 NEXT X
    RUN

### Circles with Trig

    10 GRAPHICS 4
    20 LET CX = SCRW / 2
    30 LET CY = SCRH / 2
    40 COLOR 14
    50 FOR A = 0 TO 6.28 STEP 0.05
    60 LET X = CX + INT(200 * COS(A))
    70 LET Y = CY + INT(200 * SIN(A))
    80 PLOT X, Y
    90 NEXT A
    100 PAUSE
    110 GRAPHICS 0
    RUN

## Host Disk Utility (idu)

A Python script for managing files on the ICARUS disk image from your host machine (macOS/Linux). Defaults to `disk.img` in the current directory.

### Usage

    idu list
    idu write <name> <file>
    idu read <name> [outfile]
    idu delete <name>
    idu format
    idu -f <path> list         (use a different disk image)

### Writing BASIC Programs

Create a text file with line numbers:

    10 PRINT "HELLO WORLD"
    20 FOR I = 1 TO 5
    30 PRINT I
    40 NEXT I

Write it to disk:

    idu write HELLO hello.bas

Then in ICARUS:

    LOAD "HELLO"
    RUN

### Notes

- File names are limited to 11 characters.
- Create a disk image with `dd if=/dev/zero of=disk.img bs=1M count=2`.
- Format before first use: `idu format` or `FORMAT` in ICARUS.
- Do not modify the disk image while QEMU is running.
