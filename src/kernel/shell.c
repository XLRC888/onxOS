#include "shell.h"
#include "vga.h"
#include "keyboard.h"
#include "serial.h"
#include "port.h"
#include "string.h"
#include "fs.h"
#include "commands.h"
#define LB 256
#define MA 16
#define HS 32
static char lb[LB];
static int lp = 0;
static fs_node_t *cd;
char hist[HS][LB];
int hc = 0;
static int hi = -1;
static int pr, pc;
static const char *gp(void) {
    static char p[128]; char abs[256];
    fs_to_absolute(abs, cd, "");
    p[0]=0; strcat(p,abs); strcat(p," $ "); return p;
}
static void ha(const char *l) {
    if(!*l)return;
    if(hc>0&&strcmp(hist[hc-1],l)==0)return;
    if(hc<HS)strcpy(hist[hc++],l);
    else{for(int i=1;i<HS;i++)strcpy(hist[i-1],hist[i]);strcpy(hist[HS-1],l);}
    hi=hc;
}
static int wc(char c) { return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9'); }
static int tcw(char c){return wc(c)||c=='.'||c=='/'||c=='-'||c=='_'||c=='~';}
static struct{char orig[LB],m[MA][LB],lcp[LB];int ws,wlen,idx,cnt,active;}tc;
static void tc_match(const char *w,int fw){
    int sl=-1;for(int i=0;w[i];i++)if(w[i]=='/')sl=i;
    const char *pf=w;fs_node_t *d=cd;
    if(sl>=0){char dp[MAX_PATH];if(sl==0)strcpy(dp,"/");else{strncpy(dp,w,sl);dp[sl]=0;}d=fs_resolve(dp,cd);if(!d||d->type!=FT_DIR)goto err;pf=w+sl+1;}
    int pl=strlen(pf);tc.cnt=0;
    if(fw&&sl<0){static const char *cs[]={"help","clear","pwd","ls","cd","mkdir","touch","rm","rmdir","cat","echo","cp","mv","stat","hex","ver","reboot","tau","tree","find","grep","head","cowsay","setup","exit","tail","wc","history","uname","df","du","sort","yes","sleep","seq","rev","which","tac","base64","uniq",0};for(int i=0;cs[i]&&tc.cnt<MA;i++){if(!pf[0]||strncmp(cs[i],pf,pl)==0){char b[LB];strcpy(b,cs[i]);strcpy(tc.m[tc.cnt++],b);}}}
    if(!fw||sl>=0){for(int i=0;i<d->child_count&&tc.cnt<MA;i++){if(!pf[0]||strncmp(d->children[i]->name,pf,pl)==0){char b[LB];if(sl>=0){strncpy(b,w,sl+1);b[sl+1]=0;strcat(b,d->children[i]->name);}else strcpy(b,d->children[i]->name);if(d->children[i]->type==FT_DIR)strcat(b,"/");strcpy(tc.m[tc.cnt++],b);}}}
    return;
    err:tc.cnt=0;
}
static void tc_lcp(void){if(tc.cnt<=0){tc.lcp[0]=0;return;}strcpy(tc.lcp,tc.m[0]);for(int i=1;i<tc.cnt;i++){int j=0;while(tc.lcp[j]&&tc.m[i][j]&&tc.lcp[j]==tc.m[i][j])j++;tc.lcp[j]=0;}}
static void tc_show(void){
    if(pr>=24){vga_set_cursor(24,0);vga_putchar_raw('\n');pr=23;}
    if(tc.cnt<=1)return;int x=0;
    int tr=pr+1;
    vga_set_cursor(tr,0);
    for(int i=0;i<tc.cnt&&x<79;i++){
        int hl=(i+1==tc.idx);vga_set_fg(hl?COLOR_BLACK:COLOR_WHITE);vga_set_bg(hl?COLOR_WHITE:COLOR_BLACK);
        int l=strlen(tc.m[i]);for(int j=0;j<l&&x<79;j++){vga_putchar_raw(tc.m[i][j]);x++;}
        if(x<79){vga_putchar_raw(' ');x++;}
    }
    vga_set_fg(COLOR_LIGHT_GREY);vga_set_bg(COLOR_BLACK);for(;x<79;x++)vga_putchar_raw(' ');
    vga_set_cursor(pr,pc+lp);
}
static void tc_hide(void){
    if(pr>=24)pr=23;
    int tr=pr+1;
    vga_set_cursor(tr,0);for(int i=0;i<79;i++)vga_putchar_raw(' ');
    vga_set_cursor(pr,pc+lp);
}
static void ct(int pos) {
    vga_set_cursor(pr, pc + pos); serial_write("\r"); serial_write(gp());
    for(int i=0;i<pos;i++)serial_putchar(lb[i]);
}
static void rl(const char *nl, int ol) {
    int nl2=strlen(nl);
    vga_set_cursor(pr, pc); for(int i=0;i<lp;i++)vga_putchar_raw(' ');
    vga_set_cursor(pr, pc); for(int i=0;i<nl2;i++)vga_putchar_raw(nl[i]);
    for(int i=nl2;i<ol;i++)vga_putchar_raw(' ');
    serial_write("\r");serial_write(gp());serial_write(nl);
    for(int i=nl2;i<ol;i++)serial_putchar(' ');
    for(int i=nl2;i<ol;i++)serial_putchar('\b');
    strcpy(lb,nl);lp=nl2;
}
static int exec_hist(const char *s) {
    if(s[0]!='!'||s[1]==0)return 0;
    if(s[1]=='!'){if(hc==0)return 0;strcpy((char*)s,hist[hc-1]);return 1;}
    int n=0;for(int i=1;s[i];i++){if(s[i]>='0'&&s[i]<='9')n=n*10+(s[i]-'0');else return 0;}
    if(n<1||n>hc)return 0;
    const char *cmd = hist[n-1];
    char buf[LB];strcpy(buf,cmd);
    strcpy((char*)s,buf);
    return 1;
}
static void exec(const char *cmd) {
    char buf[LB];
    strcpy(buf, cmd);
    char *a[MA];
    int ac = 0;
    char *p = buf;
    while (*p == ' ') p++;
    if (*p == '!') { if (exec_hist(buf)) { vga_writeln(buf); p = buf; } else { vga_writeln("onx: bad history"); return; } }
    while (*p == ' ') p++;
    while (*p && ac < MA) { a[ac++] = p; while (*p && *p != ' ') p++; if (*p) { *p = 0; p++; } while (*p == ' ') p++; }
    if (ac == 0) return;
    if (a[0][0] == '#') return;
    if (strcmp(a[0],"help")==0)cmd_help();
    else if(strcmp(a[0],"clear")==0)cmd_clear();
    else if(strcmp(a[0],"pwd")==0)cmd_pwd(cd);
    else if(strcmp(a[0],"ls")==0)cmd_ls(cd,ac>1?a[1]:"");
    else if(strcmp(a[0],"cd")==0)cmd_cd(&cd,ac>1?a[1]:"~");
    else if(strcmp(a[0],"mkdir")==0)cmd_mkdir(cd,ac>1?cmd+strlen(a[0]):"");
    else if(strcmp(a[0],"touch")==0)cmd_touch(cd,ac>1?cmd+strlen(a[0]):"");
    else if(strcmp(a[0],"rm")==0)cmd_rm(cd,ac>1?cmd+strlen(a[0]):"");
    else if(strcmp(a[0],"rmdir")==0)cmd_rmdir(cd,ac>1?cmd+strlen(a[0]):"");
    else if(strcmp(a[0],"cat")==0)cmd_cat(cd,ac>1?a[1]:"");
    else if(strcmp(a[0],"echo")==0){const char *ea=ac>1?cmd+strlen(a[0]):"";while(*ea==' ')ea++;cmd_echo(ea);}
    else if(strcmp(a[0],"cp")==0)cmd_cp(cd,ac>1?cmd+strlen(a[0]):"");
    else if(strcmp(a[0],"mv")==0)cmd_mv(cd,ac>1?cmd+strlen(a[0]):"");
    else if(strcmp(a[0],"stat")==0)cmd_stat(cd,ac>1?a[1]:"");
    else if(strcmp(a[0],"hex")==0)cmd_hexdump(cd,ac>1?a[1]:"");
    else if(strcmp(a[0],"ver")==0)cmd_ver();
    else if(strcmp(a[0],"reboot")==0)cmd_reboot();
    else if(strcmp(a[0],"tau")==0)cmd_tau(cd,ac>1?a[1]:"");
    else if(strcmp(a[0],"tree")==0)cmd_tree(cd,ac>1?a[1]:"");
    else if(strcmp(a[0],"find")==0)cmd_find(cd,ac>1?cmd+strlen(a[0]):"");
    else if(strcmp(a[0],"grep")==0)cmd_grep(cd,ac>1?cmd+strlen(a[0]):"");
    else if(strcmp(a[0],"cowsay")==0){const char *ca=ac>1?cmd+strlen(a[0]):"";while(*ca==' ')ca++;cmd_cowsay(ca);}
    else if(strcmp(a[0],"head")==0)cmd_head(cd,ac>1?a[1]:"");
    else if(strcmp(a[0],"tail")==0)cmd_tail(cd,ac>1?cmd+strlen(a[0]):"");
    else if(strcmp(a[0],"wc")==0)cmd_wc(cd,ac>1?a[1]:"");
    else if(strcmp(a[0],"history")==0)cmd_history();
    else if(strcmp(a[0],"uname")==0)cmd_uname();
    else if(strcmp(a[0],"df")==0)cmd_df();
    else if(strcmp(a[0],"du")==0)cmd_du(cd,ac>1?a[1]:"");
    else if(strcmp(a[0],"sort")==0)cmd_sort(cd,ac>1?a[1]:"");
    else if(strcmp(a[0],"yes")==0){const char *ya=ac>1?cmd+strlen(a[0]):"";while(*ya==' ')ya++;cmd_yes(ya);}
    else if(strcmp(a[0],"sleep")==0){const char *sl=ac>1?cmd+strlen(a[0]):"";while(*sl==' ')sl++;cmd_sleep(sl);}
    else if(strcmp(a[0],"seq")==0){const char *sq=ac>1?cmd+strlen(a[0]):"";while(*sq==' ')sq++;cmd_seq(sq);}
    else if(strcmp(a[0],"rev")==0)cmd_rev(cd,ac>1?a[1]:"");
    else if(strcmp(a[0],"which")==0)cmd_which(cd,ac>1?a[1]:"");
    else if(strcmp(a[0],"tac")==0)cmd_tac(cd,ac>1?a[1]:"");
    else if(strcmp(a[0],"base64")==0)cmd_base64(cd,ac>1?a[1]:"");
    else if(strcmp(a[0],"uniq")==0)cmd_uniq(cd,ac>1?a[1]:"");
    else if(strcmp(a[0],"setup")==0)cmd_setup(cd);
    else if(strcmp(a[0],"exit")==0){
        vga_write("saving...");vga_writeln(fs_save_disk()?"ok":"fail");
        serial_write("halt.\n");
        outw(0x604,0x2000);outw(0xB004,0x2000);
        vga_writeln("power down or reset the machine");
        for(;;)__asm__ volatile("cli;hlt");
    }
    else if(a[0][0]=='!'){}else{vga_write("onx: unknown: ");vga_writeln(a[0]);}
}
void shell_init(void) { cd=fs_get_root(); hc=0; hi=-1; vga_scrollback_init(); }
void shell_run(void) {
    const char *on = fs_get_boot_media() ? "onxIM" : "onxOS";
    vga_set_fg(COLOR_LIGHT_GREEN);vga_write(on);vga_write(" " ONX_VERSION);
    vga_set_fg(COLOR_LIGHT_GREY);vga_writeln(" - type 'help'");
    vga_writeln("");
    while(1){
        const char *prompt=gp();vga_write(prompt);
        pr=vga_get_cursor_row();pc=vga_get_cursor_col();
        lp=0;lb[0]=0;tc.active=0;int done=0;const char *saved=0;
        while(!done){char c;if(!keyboard_getchar(&c))continue;
            if(c==KEY_PGUP){if(tc.active)tc_hide();tc.active=0;vga_scrollback_up();continue;}
            if(c==KEY_PGDN){if(tc.active)tc_hide();tc.active=0;vga_scrollback_down();if(!vga_in_scrollback()){vga_set_cursor(pr,pc+lp);}continue;}
            if(vga_in_scrollback()){vga_scrollback_exit();vga_set_cursor(pr,pc+lp);}
            if(c=='\n'||c=='\r'){if(tc.active){tc_hide();tc.active=0;}lb[strlen(lb)]=0;vga_putchar('\n');done=1;}
            else if(c=='\b'||c==KEY_DELETE){if(tc.active)tc_hide();tc.active=0;if(lp>0){int ol=strlen(lb);int l2=ol;for(int i=lp-1;i<l2;i++)lb[i]=lb[i+1];lp--;int sp=lp;rl(lb,ol);lp=sp;ct(lp);}}
            else if(c==KEY_LEFT){if(tc.active)tc_hide();tc.active=0;if(lp>0){lp--;ct(lp);}}
            else if(c==KEY_RIGHT){if(tc.active)tc_hide();tc.active=0;int l=strlen(lb);if(lp<l){lp++;ct(lp);}}
            else if(c==KEY_UP){if(tc.active)tc_hide();tc.active=0;if(hi>0){hi--;saved=hist[hi];int ol=strlen(lb);rl(saved,ol);ct(lp);}}
            else if(c==KEY_DOWN){if(tc.active)tc_hide();tc.active=0;if(hi<hc-1){hi++;saved=hist[hi];int ol=strlen(lb);rl(saved,ol);ct(lp);}else if(hi==hc-1){hi=hc;int ol=strlen(lb);rl("",ol);ct(lp);}}
            else if(c==KEY_WORD_LEFT){if(tc.active)tc_hide();tc.active=0;if(lp>0){int np=lp-1;while(np>0&&!wc(lb[np]))np--;while(np>0&&wc(lb[np-1]))np--;lp=np;ct(lp);}}
            else if(c==KEY_WORD_RIGHT){if(tc.active)tc_hide();tc.active=0;int l=strlen(lb);if(lp<l){int np=lp;while(np<l&&wc(lb[np]))np++;while(np<l&&!wc(lb[np]))np++;lp=np;ct(lp);}}
            else if(c==KEY_WORD_DELETE){if(tc.active)tc_hide();tc.active=0;if(lp>0){int ol=strlen(lb);int s=lp-1;while(s>0&&!wc(lb[s]))s--;while(s>0&&wc(lb[s-1]))s--;int l2=lp-s;for(int i=s;i+l2<LB;i++)lb[i]=lb[i+l2];lp=s;int sp=lp;rl(lb,ol);lp=sp;ct(lp);}}
            else if(c==KEY_TAB||c=='\t'){
                if(tc.active)tc_hide(); int ol=strlen(lb);
                int ws=lp;while(ws>0&&tcw(lb[ws-1]))ws--;int wlen=lp-ws;
                if(wlen==0&&ws==0){tc.active=0;continue;}
                char word[LB];strncpy(word,lb+ws,wlen);word[wlen]=0;
                if(!tc.active||strncmp(word,tc.orig+tc.ws,tc.wlen)!=0){
                    strcpy(tc.orig,lb);tc.ws=ws;tc.wlen=wlen;tc.active=1;
                    tc_match(word,ws==0);
                    if(tc.cnt==0){tc.active=0;continue;}
                    if(tc.cnt==1){tc.lcp[0]=0;tc.idx=1;}
                    else{tc_lcp();if(strcmp(tc.lcp,word)==0){tc.lcp[0]=0;tc.idx=1;}else tc.idx=0;}
                }else{
                    if(tc.cnt==1&&wlen>0&&word[wlen-1]=='/'){
                        strcpy(tc.orig,lb);tc.ws=ws;tc.wlen=wlen;
                        tc_match(word,ws==0);
                        if(tc.cnt==0){tc.active=0;continue;}
                        if(tc.cnt==1){tc.lcp[0]=0;tc.idx=1;}
                        else{tc_lcp();if(strcmp(tc.lcp,word)==0){tc.lcp[0]=0;tc.idx=1;}else tc.idx=0;}
                    }else if(tc.cnt==1)continue;
                    else{
                        strcpy(lb,tc.orig);lp=strlen(lb);
                        if(tc.idx==0)tc.idx=1;else{tc.idx++;if(tc.idx>tc.cnt){tc.idx=1;tc.lcp[0]=0;}}
                    }
                }
                char comp[LB];
                if(tc.lcp[0]&&tc.idx==0)strcpy(comp,tc.lcp);else strcpy(comp,tc.m[tc.idx-1]);
                char nb[LB];strcpy(nb,tc.orig);
                int ow=tc.wlen,nw=strlen(comp),rs=strlen(nb+tc.ws+ow);
                if(nw>ow){for(int i=rs;i>=0;i--)nb[tc.ws+nw+i]=nb[tc.ws+ow+i];}else for(int i=0;i<=rs;i++)nb[tc.ws+nw+i]=nb[tc.ws+ow+i];
                for(int i=0;i<nw;i++)nb[tc.ws+i]=comp[i];
                rl(nb,ol);lp=tc.ws+nw;ct(lp);tc_show();
            }
            else if(c>=32&&c<127&&lp<LB-1){if(tc.active)tc_hide();tc.active=0;int l=strlen(lb);if(lp<l){int ol=strlen(lb);for(int i=l;i>lp;i--)lb[i]=lb[i-1];lb[l+1]=0;lb[lp]=c;lp++;int sp=lp;rl(lb,ol);lp=sp;ct(lp);}else{lb[lp]=c;lb[lp+1]=0;lp++;vga_putchar(c);}}
        }
        if(strlen(lb)>0){ha(lb);exec(lb);}
        hi=hc;
    }
}
