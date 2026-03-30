/*
 * keyboard.h - Keyboard input API declarations
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

#ifndef KEYBOARD_H
#define KEYBOARD_H

/* Poll for a keypress. Blocks until a key is pressed.
   Returns the ASCII character, or 0 for non-printable keys. */
char keyboard_poll(void);

/* Non-blocking check if Escape was pressed. Returns 1 if so. */
int keyboard_escape_pressed(void);

#endif
