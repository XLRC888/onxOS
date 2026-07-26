#include "ll.h"
#include "vga.h"
#include "serial.h"
#include "fs.h"
#include "string.h"
#include "keyboard.h"
#include "pit.h"
#define va_list __builtin_va_list
#define va_start __builtin_va_start
#define va_end __builtin_va_end
#define va_arg __builtin_va_arg

int lib_imported[4];
int func_count;
FuncDef funcs[MAX_FUNCS];

double __floatsidf(int a) { return (double)a; }
int __fixdfsi(double a) { return (int)a; }

Value make_num(double n) {
    Value v = {VAL_NUM, {.num=n}};
    return v;
}
Value make_str(const char *s) {
    Value v = {VAL_STR, {.str=sdup(s)}};
    return v;
}
Value make_list(void) {
    Value v = {VAL_LIST, {.list={NULL, 0, 0}}};
    return v;
}

int lib_idx(const char *name) {
    if (!strcmp(name, "math")) return 0;
    if (!strcmp(name, "string")) return 1;
    if (!strcmp(name, "sys")) return 2;
    if (!strcmp(name, "scr")) return 3;
    return -1;
}
Value lib_dispatch(const char *lib, const char *fn, int argc, char **args, int line) {
    (void)line;
    if (!strcmp(lib, "sys")) {
        if (!strcmp(fn, "exit")) { error_occurred = 1; ll_longjmp(&error_jmp, 1); }
    }
    if (!strcmp(lib, "scr")) {
        if (!strcmp(fn, "clear")) { vga_clear(); return make_num(0); }
        if (!strcmp(fn, "gotoxy")) {
            if (argc < 2) fatal("scr@gotoxy: need x y");
            int x = (int)strtod(args[0], NULL);
            int y = (int)strtod(args[1], NULL);
            vga_set_cursor(y, x); return make_num(0);
        }
        if (!strcmp(fn, "color")) {
            if (argc < 2) fatal("scr@color: need fg bg");
            int fg = (int)strtod(args[0], NULL);
            int bg = (int)strtod(args[1], NULL);
            vga_set_fg(fg); vga_set_bg(bg); return make_num(0);
        }
        if (!strcmp(fn, "key")) {
            char c; return make_num(keyboard_getchar(&c) ? (double)(unsigned char)c : 0);
        }
        if (!strcmp(fn, "ticks")) { return make_num((double)pit_get_ticks()); }
        fatal("scr: unknown function");
    }
    if (!strcmp(lib, "math")) return make_num(0);
    if (!strcmp(lib, "string")) return make_num(0);
    fatal("little-lil: library not available");
    return undef_val;
}

int memcmp(const void *s1, const void *s2, unsigned long n) {
    const unsigned char *a = s1, *b = s2;
    for (unsigned long i = 0; i < n; i++) { if (a[i] != b[i]) return a[i] - b[i]; }
    return 0;
}
int tolower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
int isalnum(int c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
int isdigit(int c) { return c >= '0' && c <= '9'; }
int isalpha(int c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
double strtod(const char *str, char **endptr) {
    double res = 0.0; int neg = 0, i = 0;
    while (str[i] == ' ') i++;
    if (str[i] == '-') { neg = 1; i++; } else if (str[i] == '+') i++;
    while (str[i] >= '0' && str[i] <= '9') { res = res * 10.0 + (str[i] - '0'); i++; }
    if (str[i] == '.') { i++; double frac = 1.0; while (str[i] >= '0' && str[i] <= '9') { frac /= 10.0; res += (str[i] - '0') * frac; i++; } }
    if (str[i] == 'e' || str[i] == 'E') { i++; int e_neg = 0, exp = 0; if (str[i] == '-') { e_neg = 1; i++; } else if (str[i] == '+') i++; while (str[i] >= '0' && str[i] <= '9') { exp = exp * 10 + (str[i] - '0'); i++; } if (e_neg) while (exp-- > 0) res /= 10.0; else while (exp-- > 0) res *= 10.0; }
    if (endptr) *endptr = (char *)(str + i);
    return neg ? -res : res;
}
int snprintf(char *buf, unsigned long sz, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); int r = 0;
    for (unsigned long i = 0; fmt[i] && r < (int)sz - 1; i++) {
        if (fmt[i] == '%') {
            i++;
            switch (fmt[i]) {
                case 's': { const char *s = va_arg(ap, const char *); while (*s && r < (int)sz - 1) buf[r++] = *s++; break; }
                case 'd': { int v = va_arg(ap, int); if (v < 0) { buf[r++] = '-'; v = -v; } char tb[16]; int ti = 0; if (v == 0) { tb[ti++] = '0'; } else { while (v > 0) { tb[ti++] = '0' + (v % 10); v /= 10; } } while (ti > 0 && r < (int)sz - 1) buf[r++] = tb[--ti]; break; }
                case 'c': { int c2 = va_arg(ap, int); if (r < (int)sz - 1) buf[r++] = c2; break; }
                default: if (r < (int)sz - 1) buf[r++] = fmt[i]; break;
            }
        } else { buf[r++] = fmt[i]; }
    }
    buf[r] = 0; va_end(ap); return r;
}

static int ll_inited = 0;
static void print_cb(const char *s, void *u) {
    (void)u;
    vga_write(s);
    if (serial_is_present()) serial_write(s);
}
void cmd_lil(fs_node_t *cwd, const char *arg) {
    if (!arg || !*arg) { vga_writeln("lil: usage: lil <code>"); return; }
    if (!ll_inited) { ll_init(); ll_set_print_cb(print_cb, NULL); ll_inited = 1; }
    fs_node_t *nd = fs_resolve(arg, cwd);
    if (nd && nd->type == FT_FILE) {
        if (ll_eval(nd->content) != 0) { vga_write("lil: "); vga_writeln(ll_get_error()); }
        return;
    }
    char *buf = malloc(strlen(arg) + 1);
    if (!buf) { vga_writeln("lil: oom"); return; }
    strcpy(buf, arg);
    if (ll_eval(buf) != 0) { vga_write("lil: "); vga_writeln(ll_get_error()); }
    free(buf);
}
