#include "pit.h"
#include "isr.h"
#include "port.h"
static volatile uint32_t pit_ticks = 0;
static void pit_cb(registers_t *r) {
    (void)r;
    pit_ticks++;
}
void pit_init(void) {
    outb(0x43, 0x34);
    outb(0x40, 0x9C);
    outb(0x40, 0x2E);
    isr_register_callback(0, pit_cb);
    outb(0x21, 0xF8);
}
uint32_t pit_get_ticks(void) {
    return pit_ticks;
}
