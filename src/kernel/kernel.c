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
void kernel_early(unsigned int magic, unsigned int mb_info) {
    (void)magic;
    vga_init();
    serial_init();
    serial_write("start\n");
    gdt_init();
    serial_write("gdt ok\n");
    idt_init();
    serial_write("idt ok\n");
    keyboard_init();
    serial_write("kbd ok\n");
    unsigned int mod_start = 0, mod_size = 0;
    if (mb_info) {
        unsigned int *info = (unsigned int *)mb_info;
        if (info[0] & (1 << 3)) {
            unsigned int mc = info[5];
            if (mc > 0) {
                unsigned int *mod = (unsigned int *)info[6];
                mod_start = mod[0];
                mod_size = mod[1] - mod_start;
            }
        }
    }
    uint32_t heap_start = (uint32_t)&end_of_kernel + 0x1000;
    if (mod_size > 0 && mod_start + mod_size + 0x1000 > heap_start)
        heap_start = mod_start + mod_size + 0x1000;
    memory_init((void *)heap_start, HEAP_SIZE);
    fs_init();
    int loaded = 0;
    if (fs_load_disk()) {
        loaded = 1;
        serial_write("fs: disk ok\n");
    } else {
        serial_write("fs: no disk\n");
    }
    if (!loaded && fs_ata_present() && fs_seed_disk()) {
        loaded = 1;
        serial_write("fs: auto seed ok\n");
        fs_load_disk();
    }
    if (!loaded && mod_size >= 512 && fs_load_from_memory((void *)mod_start)) {
        loaded = 1;
        serial_write("fs: module ok\n");
        if (fs_save_disk()) serial_write("fs: seed ok\n");
        else serial_write("fs: seed fail\n");
    }
    fs_set_boot_media(mod_size >= 512);
    shell_init();
    vga_write(fs_get_boot_media()?"onxIM":"onxOS");vga_writeln(" ready.");
    serial_write("shell ready\n");
}
void kernel_main(void) {
    serial_write("shell run\n");
    shell_run();
}
