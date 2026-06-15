#include "vga.h"
#include "serial.h"
#include "gdt.h"
#include "idt.h"
#include "keyboard.h"
#include "memory.h"
#include "fs.h"
#include "shell.h"
#include "string.h"
#define HEAP_SIZE 0x100000
extern uint32_t end_of_kernel;

static void parse_mb2_tags(uint32_t mbi_addr, uint32_t *mod_start, uint32_t *mod_end) {
    *mod_start = 0; *mod_end = 0;
    uint32_t total_size = *(uint32_t *)mbi_addr;
    uint32_t *tag = (uint32_t *)(mbi_addr + 8);
    while ((uint32_t)tag < mbi_addr + total_size) {
        if (tag[0] == 3) {
            *mod_start = tag[2];
            *mod_end = tag[3];
        }
        if (tag[0] == 0) break;
        uint32_t sz = tag[1];
        tag = (uint32_t *)((uint8_t *)tag + (sz < 8 ? 8 : (sz + 7) & ~7));
    }
}

void kernel_early(unsigned int magic, unsigned int mbi_addr) {
    if (magic != 0x36d76289) mbi_addr = 0;
    uint32_t mod_start = 0, mod_end = 0;
    if (mbi_addr) parse_mb2_tags(mbi_addr, &mod_start, &mod_end);
    vga_init();
    serial_init();
    serial_write("start\n");
    gdt_init();
    serial_write("gdt ok\n");
    idt_init();
    serial_write("idt ok\n");
    keyboard_init();
    __asm__ volatile("sti");
    serial_write("kbd ok\n");
    uint32_t heap_start = (uint32_t)&end_of_kernel + 0x1000;
    if (mod_end && mod_end + 0x1000 > heap_start)
        heap_start = mod_end + 0x1000;
    memory_init((void *)heap_start, HEAP_SIZE);
    fs_init();
    int loaded = 0;
    if (fs_load_disk()) {
        loaded = 1;
        serial_write("fs: disk ok\n");
    } else {
        serial_write("fs: no disk\n");
    }
    if (!loaded && mod_end > mod_start && fs_load_from_memory((void *)mod_start)) {
        loaded = 1;
        serial_write("fs: module ok\n");
    }
    shell_init();
    vga_write("onxOS");vga_writeln(" ready.");
    serial_write("shell ready\n");
}

void kernel_main(void) {
    serial_write("shell run\n");
    shell_run();
}
