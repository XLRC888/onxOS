#include "vga.h"
#include "font.h"
#include "port.h"
#include "string.h"
#include "serial.h"
#include "memory.h"
#define VM 0xB8000
#define VW 80
#define VH 25
#define SB_ROWS 4096
static uint16_t *vb = (uint16_t *)VM;
static int cr = 0, cc = 0;
static uint8_t fg = COLOR_LIGHT_GREY, bg = COLOR_BLACK;
static const int af[] = {30,34,32,36,31,35,33,37,90,94,92,96,91,95,93,97};
static const int ab[] = {40,44,42,46,41,45,43,47,100,104,102,106,101,105,103,107};
static uint16_t grub_save[VW * VH];
static uint16_t *sb = NULL;
static int sb_cnt = 0;
static uint16_t live_save[VW * VH];
static int sb_view = 0;
static uint16_t vga_init_buf[VW * VH];
static uint8_t mc(void) { return fg | (bg << 4); }
static uint16_t me(char c, uint8_t col) { return (uint16_t)c | ((uint16_t)col << 8); }
static void uc(void) {
    uint16_t p = cr * VW + cc;
    outb(0x3D4, 0x0F); outb(0x3D5, (uint8_t)(p & 0xFF));
    outb(0x3D4, 0x0E); outb(0x3D5, (uint8_t)((p >> 8) & 0xFF));
}
void vga_load_font(void) {
    uint8_t sr0, sr2, sr4, gr4, gr5, gr6, gr8, cr0a;
    outb(0x3C4, 0x00); sr0 = inb(0x3C5);
    outb(0x3C4, 0x02); sr2 = inb(0x3C5);
    outb(0x3C4, 0x04); sr4 = inb(0x3C5);
    outb(0x3CE, 0x04); gr4 = inb(0x3CF);
    outb(0x3CE, 0x05); gr5 = inb(0x3CF);
    outb(0x3CE, 0x06); gr6 = inb(0x3CF);
    outb(0x3CE, 0x08); gr8 = inb(0x3CF);
    outb(0x3D4, 0x0A); cr0a = inb(0x3D5);
    outb(0x3C4, 0x00); outb(0x3C5, 0x01);
    outb(0x3C4, 0x02); outb(0x3C5, 0x04);
    outb(0x3C4, 0x04); outb(0x3C5, 0x07);
    outb(0x3C4, 0x00); outb(0x3C5, 0x03);
    outb(0x3CE, 0x04); outb(0x3CF, 0x02);
    outb(0x3CE, 0x05); outb(0x3CF, 0x00);
    outb(0x3CE, 0x06); outb(0x3CF, 0x05);
    outb(0x3CE, 0x08); outb(0x3CF, 0xFF);
    outb(0x3D4, 0x0A); outb(0x3D5, 0x00);
    volatile uint8_t *fp = (volatile uint8_t *)0xB8000;
    for (int i = 0; i < 256; i++) {
        int off = i * 32;
        for (int j = 0; j < 16; j++) fp[off + j] = builtin_font[i * 16 + j];
        for (int j = 16; j < 32; j++) fp[off + j] = 0;
    }
    outb(0x3C4, 0x00); outb(0x3C5, sr0);
    outb(0x3C4, 0x02); outb(0x3C5, sr2);
    outb(0x3C4, 0x04); outb(0x3C5, sr4);
    outb(0x3CE, 0x04); outb(0x3CF, gr4);
    outb(0x3CE, 0x05); outb(0x3CF, gr5);
    outb(0x3CE, 0x06); outb(0x3CF, gr6);
    outb(0x3CE, 0x08); outb(0x3CF, gr8);
    outb(0x3D4, 0x0A); outb(0x3D5, cr0a);
}

void vga_init(void) {
    outb(0x3D4, 0x0C); outb(0x3D5, 0x00);
    outb(0x3D4, 0x0D); outb(0x3D5, 0x00);
    inb(0x3DA); outb(0x3C0, 0x10 | 0x20);
    uint8_t ac = inb(0x3C1);
    outb(0x3C0, ac | 0x08);
    memcpy(grub_save, vb, VW * VH * 2);
    vga_clear(); uc();
}
void vga_clear(void) {
    uint8_t c = mc();
    for (int i = 0; i < VW * VH; i++) vb[i] = me(' ', c);
    cr = 0; cc = 0; uc();
    serial_write("\033[0m\033[2J\033[H"); serial_write("\033["); serial_write_dec(af[fg]); serial_write("m");
}
void vga_set_fg(enum vga_color c) { fg = c; serial_write("\033["); serial_write_dec(af[c]); serial_write("m"); }
void vga_set_bg(enum vga_color c) { bg = c; serial_write("\033["); serial_write_dec(ab[c]); serial_write("m"); }
int vga_get_cursor_row(void) { return cr; }
int vga_get_cursor_col(void) { return cc; }
void vga_set_cursor(int row, int col) {
    if (row >= VH) row = VH - 1;
    if (col >= VW) col = VW - 1;
    if (row < 0) row = 0;
    if (col < 0) col = 0;
    cr = row; cc = col; uc();
}
static void vs(void) {
    if (sb) {
        int idx = (sb_cnt % SB_ROWS) * VW;
        memcpy(&sb[idx], &vb[0], VW * 2);
        sb_cnt++;
    }
    uint8_t c = mc();
    for (int y = 0; y < VH - 1; y++)
        for (int x = 0; x < VW; x++)
            vb[y * VW + x] = vb[(y + 1) * VW + x];
    for (int x = 0; x < VW; x++)
        vb[(VH - 1) * VW + x] = me(' ', c);
    if (cr > 0) cr--;
}
void vga_putchar(char c) {
    serial_putchar(c);
    uint8_t col = mc();
    if (c == '\n') { cr++; cc = 0; }
    else if (c == '\r') { cc = 0; }
    else if (c == '\t') { int s = 4 - (cc % 4); for (int i = 0; i < s; i++) vga_putchar(' '); return; }
    else if (c == '\b') { if (cc > 0) { cc--; vb[cr * VW + cc] = me(' ', col); } uc(); return; }
    else { vb[cr * VW + cc] = me(c, col); cc++; }
    if (cc >= VW) { cc = 0; cr++; }
    if (cr >= VH) vs();
    uc();
}
void vga_putchar_raw(char c) {
    uint8_t col = mc();
    if (c == '\n') { cr++; cc = 0; }
    else if (c == '\r') { cc = 0; }
    else if (c == '\t') { int s = 4 - (cc % 4); for (int i = 0; i < s; i++) vga_putchar_raw(' '); return; }
    else if (c == '\b') { if (cc > 0) { cc--; vb[cr * VW + cc] = me(' ', col); } uc(); return; }
    else { vb[cr * VW + cc] = me(c, col); cc++; }
    if (cc >= VW) { cc = 0; cr++; }
    if (cr >= VH) vs();
    uc();
}
void vga_write(const char *str) { while (*str) vga_putchar(*str++); }
void vga_writeln(const char *str) { vga_write(str); vga_putchar('\n'); }
void vga_write_dec(int value) { char b[16]; vga_write(itoa(value, b, 10)); }
void vga_write_hex(uint32_t value) {
    char b[16]; b[0]='0';b[1]='x';
    char *p = b + 14; p[0]=0;
    for (int i=0;i<8;i++){int d=value&0xF;p[-1]=d<10?'0'+d:'a'+d-10;p--;value>>=4;}
    vga_write(b+6);
}
void vga_scrollback_init(void) {
    if (sb) return;
    sb = (uint16_t *)malloc(SB_ROWS * VW * 2);
    if (!sb) return;
    for (int i = 0; i < VH; i++) {
        int idx = (sb_cnt % SB_ROWS) * VW;
        memcpy(&sb[idx], &grub_save[i * VW], VW * 2);
        sb_cnt++;
    }
}
static void sb_render(void) {
    if (sb_view <= 0) return;
    int end = sb_cnt - sb_view;
    int start = end - VH + 1;
    uint8_t col = COLOR_LIGHT_GREY | (COLOR_BLACK << 4);
    uint8_t tilde = COLOR_DARK_GREY | (COLOR_BLACK << 4);
    for (int r = 0; r < VH - 1; r++) {
        int src_line = start + r;
        if (src_line >= 0 && src_line < end && src_line < sb_cnt) {
            int idx = (src_line % SB_ROWS) * VW;
            for (int x = 0; x < VW; x++) vb[r * VW + x] = sb[idx + x];
        } else if (src_line < 0 || src_line >= sb_cnt) {
            vb[r * VW + 0] = me('~', tilde);
            for (int x = 1; x < VW; x++) vb[r * VW + x] = me(' ', col);
        } else {
            for (int x = 0; x < VW; x++) vb[r * VW + x] = me(' ', col);
        }
    }
    uint8_t sc = COLOR_BLACK | (COLOR_LIGHT_GREY << 4);
    char *t = " SCROLLBACK  PgUp/PgDn ";
    for (int x = 0; x < VW; x++) vb[(VH - 1) * VW + x] = me(t[x % 28], sc);
    vga_set_cursor(VH - 1, 0);
}
void vga_scrollback_up(void) {
    if (!sb || sb_cnt == 0) return;
    if (sb_view == 0) memcpy(live_save, vb, VW * VH * 2);
    if (sb_view < sb_cnt + VH - 1) { sb_view++; sb_render(); }
}
void vga_scrollback_down(void) {
    if (sb_view > 1) { sb_view--; sb_render(); }
    else if (sb_view == 1) { sb_view = 0; memcpy(vb, live_save, VW * VH * 2); vga_set_cursor(VH - 1, 0); }
}
int vga_in_scrollback(void) { return sb_view > 0; }
void vga_scrollback_exit(void) {
    if (sb_view > 0) { sb_view = 0; memcpy(vb, live_save, VW * VH * 2); }
}
