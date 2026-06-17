#include "commands.h"
#include "vga.h"
#include "string.h"
#include "fs.h"
#include "serial.h"
#include "port.h"
#include "memory.h"
#include "bootblob.h"

extern uint32_t start_of_bss;

void cmd_setup(fs_node_t *cwd) {
    if (!fs_ata_present()) {
        vga_writeln("setup: no ATA disk found");
        return;
    }
    vga_write("setup: installing onxOS to disk...\n");

    vga_write("  bootloader... ");
    if (!fs_write_sectors(0, 6, bootblob)) {
        vga_writeln("FAIL");
        return;
    }
    vga_writeln("ok");

    vga_write("  kernel... ");
    uint32_t ksz = (uint32_t)&start_of_bss - 0x200000;
    uint32_t ksect = (ksz + 511) / 512;
    if (ksect > 122) { ksect = 122; vga_write("(truncated) "); }
    if (!fs_write_sectors(6, (uint8_t)ksect, (void*)0x200000)) {
        vga_writeln("FAIL");
        return;
    }
    vga_writeln("ok");
    vga_write("  kernel: ");vga_write_dec(ksect);vga_write(" sectors\n");

    vga_write("  partition... ");
    uint8_t mbr[512];
    memset(mbr, 0, 512);
    if (!fs_read_sectors(0, 1, mbr)) {
        vga_writeln("FAIL (read mbr)");
        return;
    }
    mbr[510] = 0x55;
    mbr[511] = 0xAA;
    int pi = -1;
    for (int i = 0; i < 4; i++) {
        uint8_t *e = mbr + 446 + i * 16;
        if (e[4] == 0 && pi < 0) pi = i;
    }
    if (pi < 0) { vga_writeln("no free partition slot"); return; }
    uint32_t dlba = 128;
    uint8_t *pe = mbr + 446 + pi * 16;
    pe[0] = 0x80;
    pe[4] = 0xDA;
    pe[8] = dlba & 0xFF; pe[9] = (dlba >> 8) & 0xFF;
    pe[10] = (dlba >> 16) & 0xFF; pe[11] = (dlba >> 24) & 0xFF;
    pe[12] = 0; pe[13] = 8; pe[14] = 0; pe[15] = 0;
    if (!fs_write_sectors(0, 1, mbr)) {
        vga_writeln("FAIL (write mbr)");
        return;
    }
    vga_writeln("ok");

    vga_write("  filesystem... ");
    fs_set_data_lba(dlba);
    fs_set_boot_media(0);
    if (!fs_seed_disk()) {
        vga_writeln("FAIL");
        return;
    }
    vga_writeln("ok");

    vga_set_fg(COLOR_LIGHT_GREEN);
    vga_writeln("\nsetup done. reboot to boot onxOS from this disk");
    vga_set_fg(COLOR_LIGHT_GREY);
    serial_write("setup done\n");
}
