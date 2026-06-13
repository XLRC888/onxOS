#include "isr.h"
#include "vga.h"
#include "port.h"
static isr_callback_t irq_callbacks[16];
static const char *exc_messages[] = {
    "Division By Zero", "Debug", "Non Maskable Interrupt", "Breakpoint",
    "Into Detected Overflow", "Out of Bounds", "Invalid Opcode", "No Coprocessor",
    "Double Fault", "Coprocessor Segment Overrun", "Bad TSS", "Segment Not Present",
    "Stack Fault", "General Protection Fault", "Page Fault", "Unknown Interrupt",
    "Coprocessor Fault", "Alignment Check", "Machine Check"
};
static const char *exc_reserved = "Reserved";
void isr_handler(registers_t *regs) {
    if (regs->int_no < 32) {
        vga_set_fg(COLOR_RED);
        vga_write("KERNEL PANIC: ");
        vga_write(regs->int_no < 19 ? exc_messages[regs->int_no] : exc_reserved);
        vga_write(" (int 0x");
        vga_write_hex(regs->int_no);
        vga_writeln(")");
        vga_set_fg(COLOR_LIGHT_GREY);
        for (;;) { __asm__ volatile ("cli; hlt"); }
    }
}
void irq_handler(registers_t *regs) {
    int irq = regs->int_no - 32;
    if (irq_callbacks[irq])
        irq_callbacks[irq](regs);
    if (irq >= 8) outb(0xA0, 0x20);
    outb(0x20, 0x20);
}
void isr_register_callback(int irq, isr_callback_t cb) {
    if (irq >= 0 && irq < 16)
        irq_callbacks[irq] = cb;
}
