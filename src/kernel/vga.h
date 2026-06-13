#ifndef VGA_H
#define VGA_H
#include "kernel.h"
enum vga_color {
    COLOR_BLACK, COLOR_BLUE, COLOR_GREEN, COLOR_CYAN,
    COLOR_RED, COLOR_MAGENTA, COLOR_BROWN, COLOR_LIGHT_GREY,
    COLOR_DARK_GREY, COLOR_LIGHT_BLUE, COLOR_LIGHT_GREEN,
    COLOR_LIGHT_CYAN, COLOR_LIGHT_RED, COLOR_LIGHT_MAGENTA,
    COLOR_LIGHT_BROWN, COLOR_WHITE,
};
void vga_init(void);
void vga_putchar(char c);
void vga_write(const char *str);
void vga_writeln(const char *str);
void vga_set_fg(enum vga_color c);
void vga_set_bg(enum vga_color c);
void vga_clear(void);
void vga_set_cursor(int row, int col);
int  vga_get_cursor_row(void);
int  vga_get_cursor_col(void);
void vga_write_dec(int value);
void vga_write_hex(uint32_t value);
void vga_putchar_raw(char c);
void vga_scrollback_init(void);
void vga_scrollback_up(void);
void vga_scrollback_down(void);
int  vga_in_scrollback(void);
void vga_scrollback_exit(void);
#endif
