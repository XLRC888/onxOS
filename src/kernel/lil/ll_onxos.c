#include "ll.h"
#include "vga.h"
#include "serial.h"
#include "fs.h"
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
