#include "serial.h"
#include "port.h"
#include "string.h"
#define COM1 0x3F8
static int serial_present = 0;

int serial_is_present(void) { return serial_present; }

void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
    outb(COM1 + 4, 0x1E);
    outb(COM1 + 0, 0xAE);
    if (inb(COM1 + 0) != 0xAE) return;
    outb(COM1 + 4, 0x0F);
    char dummy;
    while (serial_getchar(&dummy));
    serial_present = 1;
}
static int serial_received(void) {
    return inb(COM1 + 5) & 1;
}
static int serial_is_transmit_empty(void) {
    return inb(COM1 + 5) & 0x20;
}
void serial_putchar(char c) {
    if (!serial_present) return;
    if (c == '\n') {
        while (!serial_is_transmit_empty());
        outb(COM1, '\r');
        while (!serial_is_transmit_empty());
        outb(COM1, '\n');
        return;
    }
    if (c == '\b') {
        while (!serial_is_transmit_empty());
        outb(COM1, '\b');
        while (!serial_is_transmit_empty());
        outb(COM1, ' ');
        while (!serial_is_transmit_empty());
        outb(COM1, '\b');
        return;
    }
    while (!serial_is_transmit_empty());
    outb(COM1, c);
}
void serial_write(const char *str) {
    while (*str) serial_putchar(*str++);
}
void serial_write_dec(int value) {
    char buf[16];
    serial_write(itoa(value, buf, 10));
}
int serial_getchar(char *c) {
    if (!serial_received()) return 0;
    *c = inb(COM1 + 0);
    if (*c == '\r') *c = '\n';
    return 1;
}
