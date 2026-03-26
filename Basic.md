# ICARUS BASIC Reference

ICARUS BASIC is a BASIC interpreter running on the ICARUS OS, a bare-metal 32-bit x86 kernel. It is inspired by Atari BASIC.

## Getting Started

Build and run in QEMU:

    make clean && make sim

The system boots, displays hardware info, and drops into the BASIC prompt:

    ICARUS BASIC
     >

Commands can be entered directly (immediate mode) or stored as numbered lines (program mode).

## Variables

- Numeric variables: single letters A-Z. No DIM required.
- String variables: A$-Z$. Must be DIMmed before use.
- Arrays: A(n) or A(r,c) for 2D. Must be DIMmed before use.

## Expressions

Arithmetic: `+`, `-`, `*`, `/`, `(`, `)`

    PRINT 2 + 3 * (4 - 1)

Comparisons (used with IF): `=`, `<`, `>`, `<=`, `>=`, `<>`

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
    LET Y = X * 2 + 1
    LET A$ = "HELLO"
    LET A(5) = 42
    LET M(1,2) = 99

### DIM

Declare a string variable or array with a given size.

    DIM A$(20)
    DIM A(10)
    DIM M(3,3)

String variables must be DIMmed before assignment. Arrays are 0-based.

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

Graphics mode uses the VGA 256-color palette (0-255).

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

Switch video mode.

    GRAPHICS 0
    GRAPHICS 1

- 0 = 80x25 text mode (restores previous screen contents)
- 1 = 320x200 pixel mode with 256 colors

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

### PAUSE

Wait for any keypress before continuing.

    PAUSE

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

Press Escape to break out of a running program.

### LIST

Display the stored program.

    LIST

### CLR

Clear the stored program and all variables.

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

### Countdown

    10 FOR I = 10 TO 1 STEP -1
    20 PRINT I
    30 NEXT I
    40 PRINT "LIFTOFF!"
    RUN

### Multiplication Table

    10 FOR I = 1 TO 5
    20 FOR J = 1 TO 5
    30 LET R = I * J
    40 PRINT R, " "
    50 NEXT J
    60 PRINT ""
    70 NEXT I
    RUN

### Graphics Demo

    10 GRAPHICS 1
    20 FOR I = 0 TO 15
    30 COLOR I
    40 PLOT 0, I * 12
    50 DRAWTO 319, I * 12
    60 NEXT I
    70 PAUSE
    80 GRAPHICS 0
    RUN

### Triangle

    10 GRAPHICS 1
    20 COLOR 4
    30 PLOT 160, 20
    40 DRAWTO 260, 180
    50 DRAWTO 60, 180
    60 DRAWTO 160, 20
    70 PAUSE
    80 GRAPHICS 0
    RUN

### Interactive

    10 DIM N$(20)
    20 INPUT "NAME? ", N$
    30 INPUT "AGE? ", A
    40 PRINT "HELLO ", N$
    50 IF A >= 18 THEN PRINT "YOU ARE AN ADULT"
    60 IF A < 18 THEN PRINT "YOU ARE A KID"
    RUN

### Using Arrays

    10 DIM A(5)
    20 DATA 10, 20, 30, 40, 50
    30 FOR I = 0 TO 4
    40 READ A(I)
    50 NEXT I
    60 FOR I = 0 TO 4
    70 PRINT A(I)
    80 NEXT I
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

## Host Disk Utility (idu)

A Python script for managing files on the ICARUS disk image from your host machine (macOS/Linux). This lets you write BASIC programs in a text editor and transfer them to the virtual disk without typing them in manually.

### Usage

    ./util/idu <disk.img> <command> [args...]

### Commands

**format** — Format the disk image. Erases all files.

    ./util/idu disk.img format

**list** — List all files and free space.

    ./util/idu disk.img list

**write** — Write a host file to the disk image with the given name.

    ./util/idu disk.img write MYPROG myprogram.bas

**read** — Read a file from the disk image. Prints to stdout, or to a file if specified.

    ./util/idu disk.img read MYPROG
    ./util/idu disk.img read MYPROG output.bas

**delete** — Delete a file from the disk image.

    ./util/idu disk.img delete MYPROG

### Writing BASIC Programs

BASIC program files must include line numbers, just as you would type them in ICARUS BASIC. For example, create a file called `hello.bas`:

    10 PRINT "HELLO WORLD"
    20 FOR I = 1 TO 5
    30 PRINT I
    40 NEXT I

Then write it to the disk and run the simulator:

    ./util/idu disk.img write HELLO hello.bas
    make sim

Inside ICARUS BASIC:

    LOAD "HELLO"
    LIST
    RUN

### Notes

- File names on the ICARUS disk are limited to 11 characters.
- The disk image must exist before using idu (create one with `dd if=/dev/zero of=disk.img bs=1M count=1`).
- Format the disk before first use, either with `./util/idu disk.img format` or with the `FORMAT` command inside ICARUS BASIC.
- Changes made with idu are written directly to the disk image file. Do not modify the disk image while QEMU is running.
