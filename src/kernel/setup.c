#include "vga.h"
#include "keyboard.h"
#include "string.h"
#include "fs.h"
#include "memory.h"
#include "kernel.h"
#include "port.h"
#ifdef STUB_ONLY
void cmd_setup(fs_node_t *cwd) { (void)cwd; }
#else
#include "bootblob_data.h"
#define BOOT_BLOB_SECT 6
#define KERN_SECT(cnt) (((cnt) + 511) / 512)
static int kb(void) {
    char c;
    while (!keyboard_getchar(&c));
    return c;
}
static void wait_key(void) {
    vga_write("press any key...");
    kb();
    vga_writeln("");
}
static void dump_status(void) {
    extern uint32_t end_of_kernel;
    unsigned int sz = (unsigned int)&end_of_kernel - 0x200000;
    vga_write("kernel: ");vga_write_dec(sz);vga_write(" bytes");
    vga_write(" (");vga_write_dec(KERN_SECT(sz));vga_writeln(" sectors)");
    vga_write("bootblob: ");vga_write_dec(build_bootblob_bin_len);vga_writeln(" bytes");
    vga_write("disk: ");vga_writeln(fs_present()?"detected":"none");
    if (fs_present()) {
        vga_write("data_lba: ");vga_write_dec(fs_get_data_lba());vga_writeln("");
    }
    for (int d = 0; d < 32; d++) {
        uint32_t id = 0;
        for (int f = 0; f < 8; f++) {
            uint32_t addr = 0x80000000 | (d << 11) | (f << 8) | 0;
            outl(0xCF8, addr); id = inl(0xCFC);
            if (id == 0xFFFFFFFF) { if (f == 0) break; continue; }
            addr = 0x80000000 | (d << 11) | (f << 8) | 8;
            outl(0xCF8, addr); uint32_t cc = inl(0xCFC);
            uint8_t cls = (cc >> 24) & 0xFF;
            if (cls == 1 || cls == 0x01) {
                vga_write("  ");vga_write_hex(id >> 16);vga_write(":");
                vga_write_hex(id & 0xFFFF);vga_write(" dev=");vga_write_dec(d);
                vga_write(" func=");vga_write_dec(f);
                vga_write(" class=");vga_write_hex(cc >> 16);vga_writeln("");
            }
        }
    }
}
static int do_install(int upd) {
    extern uint32_t end_of_kernel;
    vga_write(upd?"updating onxOS...":"installing onxOS to first HDD...");
    vga_writeln("");
    if (!fs_present()) { vga_writeln("no disk detected"); return 0; }
    unsigned int ksz = (unsigned int)&end_of_kernel - 0x200000;
    unsigned int ks = KERN_SECT(ksz);
    uint32_t dl = BOOT_BLOB_SECT + ks;
    unsigned char *buf = (unsigned char *)malloc(512);
    if (!buf) { vga_writeln("alloc fail"); return 0; }
    vga_write("stage 1: writing MBR...");
    if (!fs_write_sectors(0, 1, build_bootblob_bin)) { free(buf); vga_writeln("FAIL"); return 0; }
    vga_writeln("ok");
    vga_write("stage 2: writing bootloader...");
    if (!fs_write_sectors(1, 5, build_bootblob_bin + 512)) { free(buf); vga_writeln("FAIL"); return 0; }
    vga_writeln("ok");
    vga_write("kernel: writing ");vga_write_dec(ks);vga_write(" sectors...");
    if (!fs_write_sectors(BOOT_BLOB_SECT, ks, (void *)0x200000)) { free(buf); vga_writeln("FAIL"); return 0; }
    vga_writeln("ok");
    if (!upd) {
        vga_write("partition table: setting up...");
        for (int i = 0; i < 512; i++) buf[i] = 0;
        buf[446] = 0x80; buf[448] = 2; buf[450] = 0xDA;
        buf[454] = dl & 0xFF; buf[455] = (dl >> 8) & 0xFF;
        buf[456] = (dl >> 16) & 0xFF; buf[457] = (dl >> 24) & 0xFF;
        buf[458] = 0xFF; buf[459] = 0xFF; buf[460] = 0xFF; buf[461] = 0xFF;
        buf[510] = 0x55; buf[511] = 0xAA;
        uint8_t tmps[512];
        if (!fs_read_sectors(0, 1, tmps)) { free(buf); vga_writeln("FAIL"); return 0; }
        memcpy(tmps + 446, buf + 446, 64);
        if (!fs_write_sectors(0, 1, tmps)) { free(buf); vga_writeln("FAIL"); return 0; }
        vga_writeln("ok");
    }
    if (!upd) {
        vga_write("seeding FS to data partition at LBA ");
        vga_write_dec(dl);vga_write("...");
        fs_set_data_lba(dl);
        int sr = fs_seed_disk();
        vga_writeln(sr?"ok":"FAIL");
    } else vga_writeln("fs preserved");
    free(buf);
    vga_writeln("");
    vga_writeln(upd?"update complete. reboot to boot from HDD.":"install complete. reboot to boot from HDD.");
    return 1;
}
void cmd_setup(fs_node_t *cwd) {
    (void)cwd;
    if (!fs_get_boot_media()) { vga_writeln("setup only on installation media"); return; }
    vga_writeln("====================================");
    int upd = fs_get_data_lba() > 0;
    vga_writeln(upd?"        onxOS Update":"        onxOS Setup");
    vga_writeln("====================================");
    vga_writeln("");
    vga_writeln("[1] System status");
    vga_write("[2] ");vga_writeln(upd?"Update on first hard drive":"Install to first hard drive");
    vga_writeln("[3] Exit setup");
    vga_writeln("");
    vga_write("choice [1-3]: ");
    char c = kb();
    vga_writeln("");
    if (c == '1') { dump_status(); wait_key(); }
    else if (c == '2') { do_install(upd); wait_key(); }
    else vga_writeln("bye");
}
#endif
