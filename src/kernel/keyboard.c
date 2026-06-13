#include "keyboard.h"
#include "isr.h"
#include "port.h"
#include "serial.h"
static char kb[KEY_BUF_SIZE];
static int bh = 0, bt = 0;
static int ls = 0, rs = 0, caps = 0;
static int bf = 0;
static int ext = 0;
static int ps2ok = 0;
static const char sa[] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',
    0,'*',0,' ',0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};
static const char ss[] = {
    0,27,'!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,'A','S','D','F','G','H','J','K','L',':','"','~',
    0,'|','Z','X','C','V','B','N','M','<','>','?',
    0,'*',0,' ',0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};
static char eb[8];
static int el = 0;
static int er = 0;
static int push(char c) {
    int n = (bh + 1) % KEY_BUF_SIZE;
    if (n != bt) { kb[bh] = c; bh = n; return 1; }
    return 0;
}
static int mcs(const char *s, int ln) {
    if (ln < 2 || s[1] != '[') return 0;
    if (ln == 3) {
        if (s[2]=='A')return KEY_UP;if(s[2]=='B')return KEY_DOWN;
        if (s[2]=='C')return KEY_RIGHT;if(s[2]=='D')return KEY_LEFT;
        if (s[2]=='H')return KEY_HOME;if(s[2]=='F')return KEY_END;
    }
    if (ln==4&&s[2]=='1'&&s[3]=='~')return KEY_HOME;
    if (ln==4&&s[2]=='3'&&s[3]=='~')return KEY_DELETE;
    if (ln==4&&s[2]=='4'&&s[3]=='~')return KEY_END;
    if (ln==4&&s[2]=='5'&&s[3]=='~')return KEY_PGUP;
    if (ln==4&&s[2]=='6'&&s[3]=='~')return KEY_PGDN;
    if (ln==6&&s[2]=='1'&&s[3]==';'&&s[4]=='5'&&s[5]=='D')return KEY_WORD_LEFT;
    if (ln==6&&s[2]=='1'&&s[3]==';'&&s[4]=='5'&&s[5]=='C')return KEY_WORD_RIGHT;
    if (ln==4&&s[2]=='5'&&s[3]=='D')return KEY_WORD_LEFT;
    if (ln==4&&s[2]=='5'&&s[3]=='C')return KEY_WORD_RIGHT;
    return 0;
}
static int ist(char ch) { return (ch>='A'&&ch<='Z')||(ch>='a'&&ch<='z')||ch=='~'; }
static void sc(uint8_t s) {
    if (s == 0xF0) { bf = 1; return; }
    if (bf) { bf = 0; return; }
    if (s == 0xE0) { ext = 1; return; }
    if (ext) {
        ext = 0;
        if (s & 0x80) return;
        if (s == 0x48) { push(KEY_UP); return; }
        if (s == 0x49) { push(KEY_PGUP); return; }
        if (s == 0x4B) { push(KEY_LEFT); return; }
        if (s == 0x4D) { push(KEY_RIGHT); return; }
        if (s == 0x50) { push(KEY_DOWN); return; }
        if (s == 0x51) { push(KEY_PGDN); return; }
        if (s == 0x53) { push(KEY_DELETE); return; }
        return;
    }
    if (s == 0x2A) { ls = 1; return; }
    if (s == 0x36) { rs = 1; return; }
    if (s == 0xAA) { ls = 0; return; }
    if (s == 0xB6) { rs = 0; return; }
    if (s == 0x3A || s == 0x58) { caps = !caps; return; }
    if (s & 0x80) return;
    if (s == 0x47) { push(KEY_HOME); return; }
    if (s == 0x49) { push(KEY_PGUP); return; }
    if (s == 0x4F) { push(KEY_END); return; }
    if (s == 0x51) { push(KEY_PGDN); return; }
    if (s == 0x53) { push(KEY_DELETE); return; }
    int sh = ls || rs;
    char c;
    if (sh && s < sizeof(ss)) c = ss[s];
    else if (s < sizeof(sa)) c = sa[s];
    else c = 0;
    if (caps && c >= 'a' && c <= 'z') c -= 32;
    if (caps && c >= 'A' && c <= 'Z') c += 32;
    if (c) push(c);
}
static void cb(registers_t *r) { (void)r; sc(inb(0x60)); }
void keyboard_init(void) {
    for (int i = 0; i < 100; i++) { if (inb(0x64) & 1) inb(0x60); else break; }
    outb(0x64, 0xAE);
    for (int i = 0; i < 100000; i++) __asm__ volatile ("pause");
    for (int i = 0; i < 100; i++) { if (inb(0x64) & 1) inb(0x60); else break; }
    isr_register_callback(1, cb);
    ps2ok = 1;
}
int keyboard_getchar(char *c) {
    if (bh != bt) { *c = kb[bt]; bt = (bt + 1) % KEY_BUF_SIZE; return 1; }
    if (serial_is_present()) {
        if (el > 0) {
            char r2;
            if (serial_getchar(&r2)) {
                eb[el++] = r2;
                er = 0;
                if (ist(r2) || el >= 8) {
                    int k = mcs(eb, el);
                    el = 0;
                    if (k) { *c = (char)k; return 1; }
                }
                return 0;
            }
            er++;
            if (er > 500000) {
                el = 0; er = 0;
                *c = 27;
                return 1;
            }
            return 0;
        }
        char raw;
        if (serial_getchar(&raw)) {
            unsigned char u = (unsigned char)raw;
            if (u == 0x1b) { eb[0]=0x1b; el=1; er=0; return 0; }
            if (u == 0x7f) raw = '\b';
            *c = raw;
            return 1;
        }
    }
    return 0;
}
