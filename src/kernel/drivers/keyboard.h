#ifndef KEYBOARD_H
#define KEYBOARD_H
#include "kernel.h"
#define KEY_BUF_SIZE 256
#define KEY_ESC 27
#define KEY_UP -128
#define KEY_DOWN -127
#define KEY_RIGHT -126
#define KEY_LEFT -125
#define KEY_PGUP -122
#define KEY_PGDN -121
#define KEY_WORD_LEFT -120
#define KEY_WORD_RIGHT -119
#define KEY_DELETE -118
#define KEY_WORD_DELETE -117
#define KEY_TAB -116
#define KEY_HOME -115
#define KEY_END -114
#define KEY_INSERT -113
#define KEY_SLEFT -110
#define KEY_SRIGHT -109
#define KEY_SUP -108
#define KEY_SDOWN -107
void keyboard_init(void);
int keyboard_getchar(char *c);
#endif
