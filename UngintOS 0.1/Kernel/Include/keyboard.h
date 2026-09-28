#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "stdint.h"

void keyboard_init(void);
void keyboard_poll(void);

char getch(void);
int kbhit(void);

uint8_t read_scancode(void);
char scancode_to_ascii(uint8_t scancode);

int is_shift_pressed(void);
int is_capslock_on(void);

#endif