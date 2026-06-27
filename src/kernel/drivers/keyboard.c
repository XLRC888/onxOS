#include "keyboard.h"
#include "isr.h"
#include "port.h"
#include "serial.h"
#include "string.h"
static char kb[KEY_BUF_SIZE];
static int bh = 0, bt = 0;
static int ls = 0, rs = 0, caps = 0, ctrl = 0, caps_held = 0;
static int bf = 0;
static int ext = 0;
static int ps2ok = 0;

static const unsigned char en_norm[128] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',
    0,'*',0,' ',0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};
static const unsigned char en_shift[128] = {
    0,27,'!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,'A','S','D','F','G','H','J','K','L',':','"','~',
    0,'|','Z','X','C','V','B','N','M','<','>','?',
    0,'*',0,' ',0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

static const unsigned char trq_norm[128] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','g','\x81','\n',
    0,'a','s','d','f','g','h','j','k','l','s','i','`',
    0,'\x94','z','x','c','v','b','n','m',',','.','/',
    0,'*',0,' ',0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};
static const unsigned char trq_shift[128] = {
    0,27,'!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','G','\x9a','\n',
    0,'A','S','D','F','G','H','J','K','L','S','I','~',
    0,'\x99','Z','X','C','V','B','N','M','<','>','?',
    0,'*',0,' ',0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

static const unsigned char de_norm[128] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','z','u','i','o','p','\x81','+','\n',
    0,'a','s','d','f','g','h','j','k','l','\x94','\x84','^',
    0,'#','y','x','c','v','b','n','m',',','.','-',
    0,'*',0,' ',0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};
static const unsigned char de_shift[128] = {
    0,27,'!','"','\xa7','$','%','&','/','(',')','=','?','`','\b',
    '\t','Q','W','E','R','T','Z','U','I','O','P','\x9a','*','\n',
    0,'A','S','D','F','G','H','J','K','L','\x99','\x8e','\xf8',
    0,'\'','Y','X','C','V','B','N','M',';',':','_',
    0,'*',0,' ',0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

static const unsigned char fr_norm[128] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','a','z','e','r','t','y','u','i','o','p','^','$','\n',
    0,'q','s','d','f','g','h','j','k','l','m','\x97','\x82',
    0,'*','w','x','c','v','b','n',',',';',':','!',
    0,'*',0,' ',0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};
static const unsigned char fr_shift[128] = {
    0,27,'&','\x82','"','\'','(','-','\x8a','_','\x87','\x85',')','=','\b',
    '\t','A','Z','E','R','T','Y','U','I','O','P','\xa8','\xa3','\n',
    0,'Q','S','D','F','G','H','J','K','L','M','\x99','`',
    0,'\xb0','W','X','C','V','B','N','?','.','/','\xa1',
    0,'*',0,' ',0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

static const unsigned char trf_norm[128] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','f','g','g','i','i','j','\x94','p','s','u','\x81','v','\n',
    0,'a','b','c','\x87','d','e','h','k','l','m','n','s',
    0,'t','z','s','\x87','c','w','\'','x',',','.','/',
    0,'*',0,' ',0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};
static const unsigned char trf_shift[128] = {
    0,27,'!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','F','G','G','I','I','J','\x99','P','S','U','\x9a','V','\n',
    0,'A','B','C','\x80','D','E','H','K','L','M','N','S',
    0,'T','Z','S','\x80','C','W','\'','X','<','>','?',
    0,'*',0,' ',0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

static const unsigned char *sa = en_norm;
static const unsigned char *ss = en_shift;
static int cur_layout = 0;

typedef struct {
    const char *name;
    const unsigned char *norm;
    const unsigned char *shift;
} kb_layout_t;

static const kb_layout_t layouts[] = {
    {"en", en_norm, en_shift},
    {"trq", trq_norm, trq_shift},
    {"de", de_norm, de_shift},
    {"fr", fr_norm, fr_shift},
    {"trf", trf_norm, trf_shift},
};

int keyboard_set_layout(const char *name) {
    for (int i = 0; i < (int)(sizeof(layouts)/sizeof(layouts[0])); i++) {
        if (strcmp(layouts[i].name, name) == 0) {
            sa = layouts[i].norm;
            ss = layouts[i].shift;
            cur_layout = i;
            return 1;
        }
    }
    return 0;
}

const char *keyboard_get_layout_name(void) {
    return layouts[cur_layout].name;
}

int keyboard_get_layout_count(void) {
    return sizeof(layouts)/sizeof(layouts[0]);
}

const char *keyboard_get_layout_name_by_index(int i) {
    if (i < 0 || i >= keyboard_get_layout_count()) return NULL;
    return layouts[i].name;
}

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
    if (ln==6&&s[2]=='1'&&s[3]==';'&&s[4]=='2'&&s[5]=='A')return KEY_SUP;
    if (ln==6&&s[2]=='1'&&s[3]==';'&&s[4]=='2'&&s[5]=='B')return KEY_SDOWN;
    if (ln==6&&s[2]=='1'&&s[3]==';'&&s[4]=='2'&&s[5]=='C')return KEY_SRIGHT;
    if (ln==6&&s[2]=='1'&&s[3]==';'&&s[4]=='2'&&s[5]=='D')return KEY_SLEFT;
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
    if (bf) {
        bf = 0; ext = 0;
        if (s == 0x12 || s == 0x59) ls = 0;
        if (s == 0x14 || s == 0x11) ctrl = 0;
        if (s == 0x58) caps_held = 0;
        return;
    }
    if (s == 0xE0) { ext = 1; return; }
    if (ext) {
        ext = 0;
        if (s == 0x9D) { ctrl = 0; return; }
        if (s & 0x80) return;
        if (s == 0x48) { push((ls||rs)?KEY_SUP:KEY_UP); return; }
        if (s == 0x49) { push(KEY_PGUP); return; }
        if (s == 0x4B) { push((ls||rs)?KEY_SLEFT:KEY_LEFT); return; }
        if (s == 0x4D) { push((ls||rs)?KEY_SRIGHT:KEY_RIGHT); return; }
        if (s == 0x50) { push((ls||rs)?KEY_SDOWN:KEY_DOWN); return; }
        if (s == 0x51) { push(KEY_PGDN); return; }
        if (s == 0x53) { push(KEY_DELETE); return; }
        return;
    }
    if (s == 0x2A) { ls = 1; return; }
    if (s == 0x36) { rs = 1; return; }
    if (s == 0xAA) { ls = 0; return; }
    if (s == 0xB6) { rs = 0; return; }
    if (s == 0x3A || s == 0x58) { if (!caps_held) caps = !caps; caps_held = 1; return; }
    if (s == 0xBA) { caps_held = 0; return; }
    if (s == 0x1D) { ctrl = 1; return; }
    if (s == 0x9D) { ctrl = 0; return; }
    if (s & 0x80) return;
    if (s == 0x47) { push(KEY_HOME); return; }
    if (s == 0x49) { push(KEY_PGUP); return; }
    if (s == 0x4F) { push(KEY_END); return; }
    if (s == 0x51) { push(KEY_PGDN); return; }
    if (s == 0x53) { push(KEY_DELETE); return; }
    int sh = ls || rs;
    char c;
    if (sh && s < 128) c = ss[s];
    else if (s < 128) c = sa[s];
    else c = 0;
    if (caps && c >= 'a' && c <= 'z') c -= 32;
    else if (caps && c >= 'A' && c <= 'Z') c += 32;
    if (ctrl && c >= 'a' && c <= 'z') c = c - 'a' + 1;
    else if (ctrl && c >= 'A' && c <= 'Z') c = c - 'A' + 1;
    if (c) push(c);
}
static void cb(registers_t *r) { (void)r; if (inb(0x64) & 1) sc(inb(0x60)); }
void keyboard_init(void) {
    while (inb(0x64) & 1) inb(0x60);
    outb(0x64, 0xAE);
    for (int i = 0; i < 100000; i++) __asm__ volatile ("pause");
    outb(0x64, 0x20);
    for (int i = 0; i < 100000; i++) __asm__ volatile ("pause");
    uint8_t cmd = inb(0x60);
    outb(0x64, 0x60);
    for (int i = 0; i < 100000; i++) __asm__ volatile ("pause");
    outb(0x60, cmd | 0x40);
    for (int i = 0; i < 100000; i++) __asm__ volatile ("pause");
    while (inb(0x64) & 1) inb(0x60);
    isr_register_callback(1, cb);
    outb(0x21, 0xF9);
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
