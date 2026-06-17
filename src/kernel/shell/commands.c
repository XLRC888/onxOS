#include "commands.h"
#include "vga.h"
#include "string.h"
#include "memory.h"
#include "port.h"
#include "editor.h"
#include "fs.h"
#include "keyboard.h"
extern char hist[32][256];
extern int hc;
static void skip(const char **s) { while (**s == ' ') (*s)++; }
static int token(const char **s, char *out, int mx) {
    skip(s); int i=0;
    while (**s && **s != ' ' && i < mx - 1) out[i++] = **s, (*s)++;
    out[i]=0; return i>0;
}
void cmd_help(void) {
    vga_writeln("onxOS shell help\n"
        "  ls/cd/pwd        navigate directories\n"
        "  touch/mkdir      create files / directories\n"
        "  rm/rmdir         remove files (rm -r for dirs)\n"
        "  cat              print file contents\n"
        "  echo             print text\n"
        "  cp/mv            copy / move files\n"
        "  find <pat>       find files by name\n"
        "  grep <pat>       search file contents\n"
        "  head <f>         first 10 lines of file\n"
        "  tail <f>         last 10 lines (-n N)\n"
        "  wc <f>           count lines, words, bytes\n"
        "  history          show command history\n"
        "  uname            system information\n"
        "  df/du            disk usage / directory size\n"
        "  sort <f>         sort file lines\n"
        "  yes [msg]        repeat a string\n"
        "  sleep <ms>       delay milliseconds\n"
        "  seq <end>        number sequence\n"
        "  rev <f>          reverse chars per line\n"
        "  which <f>        locate a file\n"
        "  tac <f>          reverse line order\n"
        "  base64 <f>       base64 encode a file\n"
        "  uniq <f>         filter duplicate lines\n"
        "  stat <f>         file information\n"
        "  hex <f>          hex dump\n"
        "  tree [dir]       display directory tree\n"
        "  tau <f>          text editor (esc to exit)\n"
        "  cowsay [msg]     ascii cow with a message\n"
        "  clear/ver/reboot clear screen / version / reboot\n"
        "  help             you are here\n"
        "  !! or !5         re-run from history\n"
        "\nnote: ");
    vga_set_fg(COLOR_LIGHT_CYAN);
    vga_write("exit saves the filesystem and halts");
    vga_set_fg(COLOR_LIGHT_GREY);
    vga_putchar('\n');
}
void cmd_clear(void) { vga_clear(); }
void cmd_pwd(fs_node_t *cwd) { char p[MAX_PATH]; fs_to_absolute(p,cwd,""); vga_writeln(p); }
void cmd_ls(fs_node_t *cwd, const char *arg) {
    fs_node_t *d = cwd; char tk[MAX_NAME]; const char *p = arg;
    if (token(&p,tk,MAX_NAME)) { d=fs_resolve(tk,cwd); if(!d||d->type!=FT_DIR){vga_writeln("ls: bad path");return;} }
    for (int i=0;i<d->child_count;i++) {
        fs_node_t *c = d->children[i];
        vga_write("  ");
        if (c->type==FT_DIR) { vga_set_fg(COLOR_LIGHT_CYAN); vga_write(c->name); vga_set_fg(COLOR_LIGHT_GREY); vga_write("/"); }
        else vga_write(c->name);
    }
    if (d->child_count > 0) vga_putchar('\n');
}
void cmd_cd(fs_node_t **cwd, const char *arg) {
    const char *p=arg; skip(&p);
    if (!*p||strcmp(p,"~")==0){*cwd=fs_get_root();return;}
    fs_node_t *d=fs_resolve(p,*cwd);
    if(!d){vga_writeln("cd: no such dir");return;}
    if(d->type!=FT_DIR){vga_writeln("cd: not a dir");return;}
    *cwd=d;
}
static void te(char *o, const char *in, int mx) {
    if(in[0]=='~'&&(in[1]=='/'||in[1]==0)){o[0]='/';if(in[1]=='/'){int i=1,j=1;while(in[i]&&j<mx-1)o[j++]=in[++i];o[j]=0;}else o[1]=0;}
    else {strncpy(o,in,mx-1);o[mx-1]=0;}
}
static void sts(char *s) { int l=strlen(s); while(l>1&&s[l-1]=='/') s[--l]=0; }
static int ps(const char *path, fs_node_t *cwd, fs_node_t **par, char *name) {
    char buf[MAX_PATH]; strcpy(buf,path); sts(buf);
    int ls=-1; for(int i=0;buf[i];i++) if(buf[i]=='/') ls=i;
    if(ls>=0) {
        if(ls==0) *par=fs_get_root();
        else { char dp[MAX_PATH]; strncpy(dp,buf,ls); dp[ls]=0; *par=fs_resolve(dp,cwd); }
        strncpy(name,buf+ls+1,MAX_NAME-1); name[MAX_NAME-1]=0;
    } else { *par=cwd; strcpy(name,buf); }
    return *par!=0;
}
void cmd_mkdir(fs_node_t *cwd, const char *arg) {
    const char *p=arg; int ok=0;
    while(1){char r[MAX_NAME];if(!token(&p,r,MAX_NAME))break;char tk[MAX_PATH];te(tk,r,MAX_PATH);
        fs_node_t *par;char name[MAX_NAME];if(!ps(tk,cwd,&par,name)){vga_write("mkdir: ");vga_write(r);vga_writeln(": parent not found");continue;}
        if(!name[0])continue;
        if(fs_find(par,name)){vga_write("mkdir: ");vga_write(r);vga_writeln(": already exists");continue;}
        if(fs_create_dir(par,name)){ok=1;continue;}
        vga_write("mkdir: ");vga_write(r);vga_writeln(": no space");}
    if(!ok)vga_writeln("mkdir: missing operand");
}
void cmd_touch(fs_node_t *cwd, const char *arg) {
    const char *p=arg; int ok=0;
    while(1){char r[MAX_NAME];if(!token(&p,r,MAX_NAME))break;char tk[MAX_PATH];te(tk,r,MAX_PATH);
        fs_node_t *par;char name[MAX_NAME];if(!ps(tk,cwd,&par,name)){vga_write("touch: ");vga_write(r);vga_writeln(": bad path");continue;}
        if(!name[0])continue;
        if(fs_create_file(par,name)){ok=1;continue;}
        vga_write("touch: ");vga_write(r);vga_writeln(": failed");}
    if(!ok)vga_writeln("touch: missing operand");
}
static int rr(fs_node_t *p, const char *n, int rec) {
    for(int i=0;i<p->child_count;i++){
        if(strcmp(p->children[i]->name,n)!=0)continue;
        fs_node_t *nd=p->children[i];
        if(nd->type==FT_DIR&&nd->child_count>0){if(!rec)return -1;while(nd->child_count>0){if(!rr(nd,nd->children[0]->name,1))return 0;}}
        fs_delete(p,n);return 1;
    }
    return 0;
}
void cmd_rm(fs_node_t *cwd, const char *arg) {
    const char *p=arg; int rec=0; skip(&p);
    if(*p=='-'){if(p[1]=='r')rec=1;while(*p&&*p!=' ')p++;}
    int ok=0;
    while(1){char r[MAX_NAME];if(!token(&p,r,MAX_NAME))break;char tk[MAX_PATH];te(tk,r,MAX_PATH);
        fs_node_t *par;char name[MAX_NAME];if(!ps(tk,cwd,&par,name)){vga_write("rm: ");vga_write(r);vga_writeln(": not found");continue;}
        if(!name[0])continue;
        if(!rec){fs_node_t*t=fs_resolve(tk,cwd);if(t&&t->type==FT_DIR){vga_write("rm: ");vga_write(r);vga_writeln(": is a dir, use rm -r");continue;}}
        int ret=rr(par,name,rec);
        if(ret==1){ok=1;continue;}if(ret==-1){vga_write("rm: ");vga_write(r);vga_writeln(": not empty, use rm -r");continue;}
        vga_write("rm: ");vga_write(r);vga_writeln(": not found");}
    if(!ok)vga_writeln("rm: missing operand");
}
void cmd_rmdir(fs_node_t *cwd, const char *arg) {
    const char *p=arg; int ok=0;
    while(1){char r[MAX_NAME];if(!token(&p,r,MAX_NAME))break;char tk[MAX_PATH];te(tk,r,MAX_PATH);
        fs_node_t *par;char name[MAX_NAME];if(!ps(tk,cwd,&par,name)){vga_write("rmdir: ");vga_write(r);vga_writeln(": not found");continue;}
        if(!name[0])continue;int ret=rr(par,name,0);
        if(ret==1){ok=1;continue;}if(ret==-1){vga_write("rmdir: ");vga_write(r);vga_writeln(": not empty");continue;}
        vga_write("rmdir: ");vga_write(r);vga_writeln(": not found");}
    if(!ok)vga_writeln("rmdir: missing operand");
}
void cmd_cat(fs_node_t *cwd, const char *arg) {
    char tk[MAX_NAME]; const char *p=arg;
    if(!token(&p,tk,MAX_NAME)){vga_writeln("cat: missing operand");return;}
    fs_node_t *nd=fs_resolve(tk,cwd);if(!nd){vga_writeln("cat: not found");return;}
    if(nd->type!=FT_FILE){vga_writeln("cat: is a dir");return;}
    vga_writeln(nd->content);
}
void cmd_echo(const char *arg) { const char *p=arg; skip(&p); vga_writeln(p); }
void cmd_cp(fs_node_t *cwd, const char *arg) {
    char src[MAX_NAME],rdst[MAX_NAME]; const char *p=arg;
    if(!token(&p,src,MAX_NAME)||!token(&p,rdst,MAX_NAME)){vga_writeln("cp: src dst");return;}
    fs_node_t *sn=fs_resolve(src,cwd);if(!sn){vga_writeln("cp: src not found");return;}
    char dst[MAX_PATH];te(dst,rdst,MAX_PATH);
    fs_node_t *dp;char dn[MAX_NAME];
    if(!ps(dst,cwd,&dp,dn)||!dn[0]){vga_writeln("cp: bad dst");return;}
    if(sn->type==FT_DIR){fs_node_t*nd=fs_create_dir(dp,dn);if(!nd){vga_writeln("cp: fail");return;}
        for(int i=0;i<sn->child_count;i++){fs_node_t*child=sn->children[i];
            if(child->type==FT_FILE){fs_node_t*nf=fs_create_file(nd,child->name);if(nf)strcpy(nf->content,child->content);}
            else{fs_node_t*subd=fs_create_dir(nd,child->name);if(subd)for(int j=0;j<child->child_count;j++){fs_node_t*gc=child->children[j];if(gc->type==FT_FILE){fs_node_t*nf=fs_create_file(subd,gc->name);if(nf)strcpy(nf->content,gc->content);}}}
        }}
    else{fs_node_t*nf=fs_create_file(dp,dn);if(!nf){vga_writeln("cp: fail");return;}strcpy(nf->content,sn->content);}
}
void cmd_mv(fs_node_t *cwd, const char *arg) {
    char src[MAX_NAME],rdst[MAX_NAME]; const char *p=arg;
    if(!token(&p,src,MAX_NAME)||!token(&p,rdst,MAX_NAME)){vga_writeln("mv: src dst");return;}
    fs_node_t *sn=fs_resolve(src,cwd);if(!sn){vga_writeln("mv: src not found");return;}
    fs_node_t *sp=sn->parent;char dst[MAX_PATH];te(dst,rdst,MAX_PATH);
    fs_node_t *dp;char dn[MAX_NAME];
    if(!ps(dst,cwd,&dp,dn)||!dn[0]){vga_writeln("mv: bad dst");return;}
    if(sp&&fs_is_child_of(sn,dp)){vga_writeln("mv: cannot move into self");return;}
    if(fs_find(dp,dn)){vga_writeln("mv: dst exists");return;}
    if(dp->child_count>=MAX_CHILDREN){vga_writeln("mv: dst full");return;}
    int idx=-1;for(int i=0;i<sp->child_count;i++)if(sp->children[i]==sn){idx=i;break;}
    if(idx<0)return;
    for(int i=idx;i<sp->child_count-1;i++)sp->children[i]=sp->children[i+1];sp->child_count--;
    sn->parent=dp;dp->children[dp->child_count++]=sn;strcpy(sn->name,dn);
}
void cmd_stat(fs_node_t *cwd, const char *arg) {
    char tk[MAX_NAME];const char *p=arg;
    if(!token(&p,tk,MAX_NAME)){vga_writeln("stat: missing");return;}
    fs_node_t *nd=fs_resolve(tk,cwd);if(!nd){vga_writeln("stat: not found");return;}
    vga_write("  Name: ");vga_writeln(nd->name);
    vga_write("  Type: ");vga_writeln(nd->type==FT_DIR?"dir":"file");
    vga_write("  Size: ");vga_write_dec(strlen(nd->content));vga_putchar('\n');
    vga_write("  Children: ");vga_write_dec(nd->child_count);vga_putchar('\n');
}
void cmd_hexdump(fs_node_t *cwd, const char *arg) {
    char tk[MAX_NAME];const char *p=arg;
    if(!token(&p,tk,MAX_NAME)){vga_writeln("hex: missing");return;}
    fs_node_t *nd=fs_resolve(tk,cwd);if(!nd||nd->type!=FT_FILE){vga_writeln("hex: not a file");return;}
    const char *c=nd->content;int l=0;while(l<MAX_CONTENT&&c[l])l++;
    int lines=0;
    for(int i=0;i<l;i+=16){
        if(lines>=20){vga_write("-- more (space/enter=next, q=quit) --");char kc;while(keyboard_getchar(&kc)){}while(!keyboard_getchar(&kc)){}if(kc=='q'||kc=='Q'||kc==KEY_ESC){vga_putchar('\n');return;}vga_putchar('\n');lines=0;}
        vga_write_hex(i);vga_write("  ");
        for(int j=0;j<16;j++){if(i+j<l)vga_write_hex((uint8_t)c[i+j]);else vga_write("  ");vga_write(" ");}
        vga_write(" |");for(int j=0;j<16&&i+j<l;j++){char ch=c[i+j];vga_putchar(ch>=32&&ch<127?ch:'.');}vga_writeln("|");
        lines++;}
}
void cmd_ver(void) {
    const char *on = fs_get_boot_media() ? "onxIM" : "onxOS";
    vga_write(on);vga_writeln(" " ONX_VERSION);
    vga_writeln("built for i686");
}
void cmd_reboot(void) {
    vga_writeln("rebooting...");
    uint8_t g=0x02;while(g&0x02)g=inb(0x64);outb(0x64,0xFE);
    for(;;)__asm__ volatile("hlt");
}
void cmd_tau(fs_node_t *cwd, const char *arg) {
    vga_writeln("[1] cmd_tau entered");
    char tk[MAX_NAME];const char *p=arg;
    if(!token(&p,tk,MAX_NAME)){vga_writeln("tau: need filename");return;}
    vga_writeln("[2] calling editor_open");
    editor_open(cwd,tk);
}
static void pt(fs_node_t *d, int depth) {
    for(int i=0;i<d->child_count;i++){
        for(int j=0;j<depth;j++)vga_write("  ");vga_write("  ");
        if(d->children[i]->type==FT_DIR){vga_set_fg(COLOR_LIGHT_CYAN);vga_write(d->children[i]->name);vga_write("/");vga_set_fg(COLOR_LIGHT_GREY);vga_putchar('\n');pt(d->children[i],depth+1);}
        else{vga_write(d->children[i]->name);vga_putchar('\n');}
    }
}
void cmd_tree(fs_node_t *cwd, const char *arg) {
    fs_node_t *d=cwd;char tk[MAX_NAME];const char *p=arg;
    if(token(&p,tk,MAX_NAME)){d=fs_resolve(tk,cwd);if(!d){vga_writeln("tree: bad dir");return;}}
    pt(d,0);
}
static void find_r(fs_node_t *d, const char *pfx, const char *pat, int pl) {
    for(int i=0;i<d->child_count;i++){
        fs_node_t *nd=d->children[i];
        int m=1;for(int j=0;j<pl;j++){char pc=pat[j],nc=nd->name[j];
            if(pc=='*')break;if(pc!=nc&&pc!='?'){m=0;break;}
            if(!nc){if(pc!=0&&pc!='*')m=0;break;}
        }
        if(m&&(int)strlen(nd->name)>=pl){vga_write("  ");vga_write(pfx);vga_write(nd->name);if(nd->type==FT_DIR)vga_write("/");vga_putchar('\n');}
        if(nd->type==FT_DIR){int pfl=strlen(pfx);int nml=strlen(nd->name);char np[MAX_PATH];
            if(pfl+nml+2<MAX_PATH){strcpy(np,pfx);strcat(np,nd->name);strcat(np,"/");find_r(nd,np,pat,pl);}}
    }
}
void cmd_find(fs_node_t *cwd, const char *arg) {
    char pat[MAX_NAME];const char *p=arg;
    if(!token(&p,pat,MAX_NAME)){vga_writeln("find: need pattern");return;}
    int pl=strlen(pat);
    if(pl>0&&pat[pl-1]=='*')pl--;
    char rootp[MAX_PATH];fs_to_absolute(rootp,cwd,"");find_r(cwd,rootp,pat,pl);
}
static void grep_r(fs_node_t *d, const char *pat) {
    for(int i=0;i<d->child_count;i++){
        fs_node_t *nd=d->children[i];
        if(nd->type==FT_FILE&&strstr(nd->content,pat)){vga_write("  ");vga_write(nd->name);vga_writeln(": (match)");}
        if(nd->type==FT_DIR)grep_r(nd,pat);
    }
}
void cmd_grep(fs_node_t *cwd, const char *arg) {
    char pat[MAX_NAME];const char *p=arg;
    if(!token(&p,pat,MAX_NAME)){vga_writeln("grep: need pattern");return;}
    grep_r(cwd,pat);
}
void cmd_cowsay(const char *arg) {
    const char *on = fs_get_boot_media() ? "onxIM" : "onxOS";
    const char *p=arg;skip(&p);if(!*p)p=on;
    int l=strlen(p)+2;vga_write("  ");for(int i=0;i<l;i++)vga_putchar('_');vga_putchar('\n');
    vga_write(" < ");vga_write(p);vga_writeln(" >");
    vga_write("  ");for(int i=0;i<l;i++)vga_putchar('-');vga_putchar('\n');
    vga_writeln("        \\   ^__^\n"
                "         \\  (oo)\\_______\n"
                "            (__)\\       )\\/\\\n"
                "                ||----w |\n"
                "                ||     ||");
}
void cmd_head(fs_node_t *cwd, const char *arg) {
    char tk[MAX_NAME];const char *p=arg;
    if(!token(&p,tk,MAX_NAME)){vga_writeln("head: missing operand");return;}
    fs_node_t *nd=fs_resolve(tk,cwd);if(!nd||nd->type!=FT_FILE){vga_writeln("head: not a file");return;}
    const char *c=nd->content;int line=0;
    for(int i=0;c[i]&&line<10;i++){vga_putchar(c[i]);if(c[i]=='\n')line++;}
}
void cmd_tail(fs_node_t *cwd, const char *arg) {
    char tk[MAX_NAME];const char *p=arg;int n=10;
    skip(&p);
    if(*p=='-'){p++;if(*p=='n'){p++;skip(&p);n=0;while(*p>='0'&&*p<='9')n=n*10+(*p++-'0');}skip(&p);}
    if(!token(&p,tk,MAX_NAME)){vga_writeln("tail: missing operand");return;}
    fs_node_t *nd=fs_resolve(tk,cwd);if(!nd||nd->type!=FT_FILE){vga_writeln("tail: not a file");return;}
    const char *c=nd->content;int lines[260],lc=1,wrap=0;
    lines[0]=0;for(int i=0;c[i];i++){if(c[i]=='\n'){int idx=lc%260;lines[idx]=i+1;lc++;if(lc>260)wrap=1;}}
    int avail=wrap?260:lc;if(n>avail)n=avail;
    int start=avail>=1?lines[(lc-n)%260]:0;
    for(int i=start;c[i];i++)vga_putchar(c[i]);
}
void cmd_wc(fs_node_t *cwd, const char *arg) {
    char tk[MAX_NAME];const char *p=arg;
    if(!token(&p,tk,MAX_NAME)){vga_writeln("wc: missing operand");return;}
    fs_node_t *nd=fs_resolve(tk,cwd);if(!nd||nd->type!=FT_FILE){vga_writeln("wc: not a file");return;}
    const char *c=nd->content;int lines=0,words=0,chars=0,inw=0;
    for(int i=0;c[i];i++){chars++;if(c[i]=='\n')lines++;if(c[i]==' '||c[i]=='\n'||c[i]=='\t'){if(inw){words++;inw=0;}}else inw=1;}
    if(inw)words++;
    vga_write("  ");vga_write_dec(lines);vga_write(" ");vga_write_dec(words);vga_write(" ");vga_write_dec(chars);vga_putchar('\n');
}
void cmd_history(void) {
    if(hc==0){vga_writeln("no history");return;}
    for(int i=0;i<hc;i++){vga_write("  ");vga_write_dec(i+1);vga_write("  ");vga_writeln(hist[i]);}
}
void cmd_uname(void) {
    const char *on = fs_get_boot_media() ? "onxIM" : "onxOS";
    vga_write(on);vga_writeln(" i686 unknown");
}
static void df_count(fs_node_t *d, int *files, int *dirs) {
    for(int i=0;i<d->child_count;i++){
        if(d->children[i]->type==FT_DIR){(*dirs)++;df_count(d->children[i],files,dirs);}
        else (*files)++;
    }
}
void cmd_df(void) {
    if(!fs_present()){vga_writeln("df: no disk");return;}
    int files=0,dirs=0;df_count(fs_get_root(),&files,&dirs);
    vga_write("  files: ");vga_write_dec(files);vga_putchar('\n');
    vga_write("  dirs: ");vga_write_dec(dirs);vga_putchar('\n');
    vga_write("  data_lba: ");vga_write_dec(fs_get_data_lba());vga_putchar('\n');
}
static int du_size(fs_node_t *d) {
    int sz=0;
    for(int i=0;i<d->child_count;i++){
        if(d->children[i]->type==FT_DIR)sz+=du_size(d->children[i]);
        else sz+=strlen(d->children[i]->content);
    }
    return sz;
}
void cmd_du(fs_node_t *cwd, const char *arg) {
    fs_node_t *d=cwd;char tk[MAX_NAME];const char *p=arg;
    if(token(&p,tk,MAX_NAME)){d=fs_resolve(tk,cwd);if(!d||d->type!=FT_DIR){vga_writeln("du: bad dir");return;}}
    int sz=du_size(d);
    vga_write("  ");vga_write_dec(sz);vga_writeln(" bytes");
}
void cmd_sort(fs_node_t *cwd, const char *arg) {
    char tk[MAX_NAME];const char *p=arg;
    if(!token(&p,tk,MAX_NAME)){vga_writeln("sort: missing operand");return;}
    fs_node_t *nd=fs_resolve(tk,cwd);if(!nd||nd->type!=FT_FILE){vga_writeln("sort: not a file");return;}
    const char *c=nd->content;char (*lines)[256]=(char(*)[256])malloc(256*256);if(!lines){vga_writeln("sort: oom");return;}int lc=0,ci=0;
    for(int i=0;c[i]&&lc<256;i++){
        if(c[i]=='\n'){lines[lc][ci]=0;lc++;ci=0;}
        else if(ci<255)lines[lc][ci++]=c[i];
    }
    if(ci>0&&lc<256){lines[lc][ci]=0;lc++;}
    for(int i=1;i<lc;i++){
        char tmp[256];strcpy(tmp,lines[i]);int j=i-1;
        while(j>=0&&strcmp(lines[j],tmp)>0){strcpy(lines[j+1],lines[j]);j--;}
        strcpy(lines[j+1],tmp);
    }
    for(int i=0;i<lc;i++)vga_writeln(lines[i]);
    free(lines);
}
void cmd_yes(const char *arg) {
    const char *p=arg;skip(&p);
    const char *s=*p?p:"y";
    for(int i=0;i<100;i++)vga_writeln(s);
}
void cmd_sleep(const char *arg) {
    const char *p=arg;skip(&p);
    int ms=0;while(*p>='0'&&*p<='9')ms=ms*10+(*p++-'0');
    if(ms<=0)ms=1000;
    for(volatile uint32_t i=0;i<(uint32_t)ms*8000;i++)__asm__ volatile("nop");
}
void cmd_seq(const char *arg) {
    const char *p=arg;int start=1,end=0;
    char t1[32],t2[32];
    if(!token(&p,t1,32)){vga_writeln("seq: need number");return;}
    if(token(&p,t2,32)){start=atoi(t1);end=atoi(t2);}
    else{end=atoi(t1);}
    if(start>end){int t=start;start=end;end=t;}
    for(int i=start;i<=end;i++){vga_write_dec(i);vga_putchar('\n');}
}
void cmd_rev(fs_node_t *cwd, const char *arg) {
    char tk[MAX_NAME];const char *p=arg;
    if(!token(&p,tk,MAX_NAME)){vga_writeln("rev: missing operand");return;}
    fs_node_t *nd=fs_resolve(tk,cwd);if(!nd||nd->type!=FT_FILE){vga_writeln("rev: not a file");return;}
    const char *c=nd->content;char line[256];int ci=0;
    for(int i=0;c[i];i++){
        if(c[i]=='\n'){line[ci]=0;for(int j=ci-1;j>=0;j--)vga_putchar(line[j]);vga_putchar('\n');ci=0;}
        else if(ci<255)line[ci++]=c[i];
    }
    if(ci>0){line[ci]=0;for(int j=ci-1;j>=0;j--)vga_putchar(line[j]);}
}
void cmd_which(fs_node_t *cwd, const char *arg) {
    char tk[MAX_NAME];const char *p=arg;
    if(!token(&p,tk,MAX_NAME)){vga_writeln("which: missing operand");return;}
    fs_node_t *nd=fs_resolve(tk,cwd);
    if(!nd){vga_writeln("which: not found");return;}
    char abs[MAX_PATH];fs_to_absolute(abs,cwd,tk);vga_writeln(abs);
}
void cmd_tac(fs_node_t *cwd, const char *arg) {
    char tk[MAX_NAME];const char *p=arg;
    if(!token(&p,tk,MAX_NAME)){vga_writeln("tac: missing operand");return;}
    fs_node_t *nd=fs_resolve(tk,cwd);if(!nd||nd->type!=FT_FILE){vga_writeln("tac: not a file");return;}
    const char *c=nd->content;int lc=0,ci=0;
    int ms=256,ml=256;char *buf=(char*)malloc(ms*ml);if(!buf){vga_writeln("tac: oom");return;}
    for(int i=0;c[i]&&lc<ms;i++){
        if(c[i]=='\n'){buf[lc*ml+ci]=0;lc++;ci=0;}
        else if(ci<ml-1)buf[lc*ml+ci++]=c[i];
    }
    for(int i=lc-1;i>=0;i--)vga_writeln(buf+i*ml);
    free(buf);
}
void cmd_base64(fs_node_t *cwd, const char *arg) {
    char tk[MAX_NAME];const char *p=arg;
    if(!token(&p,tk,MAX_NAME)){vga_writeln("base64: missing operand");return;}
    fs_node_t *nd=fs_resolve(tk,cwd);if(!nd||nd->type!=FT_FILE){vga_writeln("base64: not a file");return;}
    const char *c=nd->content;int l=strlen(c);
    static const char t[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for(int i=0;i<l;i+=3){
        uint32_t n=((uint32_t)(uint8_t)c[i])<<16;
        if(i+1<l)n|=((uint32_t)(uint8_t)c[i+1])<<8;
        if(i+2<l)n|=(uint32_t)(uint8_t)c[i+2];
        vga_putchar(t[(n>>18)&63]);vga_putchar(t[(n>>12)&63]);
        vga_putchar(i+1<l?t[(n>>6)&63]:'=');
        vga_putchar(i+2<l?t[n&63]:'=');
    }
    vga_putchar('\n');
}
void cmd_uniq(fs_node_t *cwd, const char *arg) {
    char tk[MAX_NAME];const char *p=arg;
    if(!token(&p,tk,MAX_NAME)){vga_writeln("uniq: missing operand");return;}
    fs_node_t *nd=fs_resolve(tk,cwd);if(!nd||nd->type!=FT_FILE){vga_writeln("uniq: not a file");return;}
    const char *c=nd->content;char prev[256]="";char cur[256];int ci=0,first=1;
    for(int i=0;c[i];i++){
        if(c[i]=='\n'){cur[ci]=0;
            if(first||strcmp(cur,prev)!=0){vga_writeln(cur);strcpy(prev,cur);first=0;}
            ci=0;
        }else if(ci<255)cur[ci++]=c[i];
    }
}
