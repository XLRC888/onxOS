#ifndef SERIAL_H
#define SERIAL_H
#include "kernel.h"
int serial_is_present(void);
void serial_init(void);
void serial_write(const char *str);
void serial_putchar(char c);
void serial_write_dec(int value);
int serial_getchar(char *c);
#endif
