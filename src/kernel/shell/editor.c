#include "editor.h"
#include "vga.h"
#include "keyboard.h"
#include "port.h"
#include "string.h"
#include "memory.h"
#include "serial.h"
#define EL 512
#define EC 256
#define VR 23
#define TB 1024
typedef struct {
    char *l[EL];
    int lc, cr, cc, tl, dirty, lnv;
    char fn[64];
    fs_node_t *nd;
    int sel, sl, sc;
    char clip[TB];
} ed_t;
static ed_t ed;
static uint16_t sv[2000];
static int cdr, cdc;
static int pw(void) {
    if (!ed.lnv) return 0;
    int t = ed.lc, n = 1;
    while (t >= 10) { n++; t /= 10; }
    return n + 1;
}
static int wc(char c) { return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9'); }
static void wl(void) {
    if(ed.cc==0){if(ed.cr>0){ed.cr--;ed.cc=strlen(ed.l[ed.cr]);if(ed.cr<ed.tl)ed.tl=ed.cr;}return;}
    char *ln=ed.l[ed.cr];int c=ed.cc-1;while(c>0&&!wc(ln[c]))c--;if(!wc(ln[c])){ed.cc=c;return;}
    while(c>0&&wc(ln[c-1]))c--;ed.cc=c;
}
static void wr(void) {
    char *ln=ed.l[ed.cr];int l=strlen(ln);
    if(ed.cc>=l){if(ed.cr<ed.lc-1){ed.cr++;ed.cc=0;if(ed.cr-ed.tl>=VR)ed.tl++;}return;}
    int c=ed.cc;while(c<l&&wc(ln[c]))c++;while(c<l&&!wc(ln[c]))c++;ed.cc=c;
}
static void copy(void) {
    if (!ed.sel) return;
    int s_ln,e_ln,s_cl,e_cl;
    if(ed.sl<ed.cr||(ed.sl==ed.cr&&ed.sc<ed.cc)){s_ln=ed.sl;s_cl=ed.sc;e_ln=ed.cr;e_cl=ed.cc;}
    else{s_ln=ed.cr;s_cl=ed.cc;e_ln=ed.sl;e_cl=ed.sc;}
    int tp=0;ed.clip[0]=0;
    for(int i=s_ln;i<=e_ln&&tp<TB-1;i++){
        int st=(i==s_ln)?s_cl:0;
        int en=(i==e_ln)?e_cl:strlen(ed.l[i]);
        int ln=en-st;
        if(ln>0&&tp+ln>TB-1)ln=TB-1-tp;
        if(ln>0){strncpy(ed.clip+tp,ed.l[i]+st,ln);tp+=ln;}
        if(i<e_ln&&tp<TB-1)ed.clip[tp++]='\n';
    }
    ed.clip[tp]=0;
}
static void paste(void) {
    if(ed.clip[0]==0)return;
    ed.sel=0;
    char *p=ed.clip;
    char *nl=strchr(p,'\n');
    int fl=nl?(int)(nl-p):strlen(p);
    char rest[EC];
    char *ln=ed.l[ed.cr];int llen=strlen(ln);
    strcpy(rest,ln+ed.cc);ln[ed.cc]=0;
    int nllen=strlen(ln);
    if(nllen+fl<EC){strncpy(ln+nllen,p,fl);ln[nllen+fl]=0;}
    if(!nl){
        nllen=strlen(ln);
        int rl=strlen(rest);
        if(nllen+rl<EC)strcpy(ln+nllen,rest);
        ed.cc+=fl;ed.dirty=1;return;
    }
    p=nl+1;
    while(1){
        nl=strchr(p,'\n');
        int sl=nl?(int)(nl-p):strlen(p);
        if(nl){
            if(ed.lc<EL){
                char*nl2=(char*)malloc(EC+1);
                if(nl2){strncpy(nl2,p,sl);nl2[sl]=0;
                    for(int i=ed.lc;i>ed.cr+1;i--)ed.l[i]=ed.l[i-1];
                    ed.l[ed.cr+1]=nl2;ed.lc++;ed.cr++;}
            }p=nl+1;
        }else{
            if(ed.lc<EL){
                char*nl2=(char*)malloc(EC+1);
                if(nl2){strncpy(nl2,p,sl);
                    int rl=strlen(rest);
                    if(sl+rl<EC)strcpy(nl2+sl,rest);
                    else if(sl<EC)strncpy(nl2+sl,rest,EC-sl);
                    nl2[EC]=0;
                    for(int i=ed.lc;i>ed.cr+1;i--)ed.l[i]=ed.l[i-1];
                    ed.l[ed.cr+1]=nl2;ed.lc++;ed.cr++;ed.cc=sl;}
            }break;
        }
    }
    if(ed.cr-ed.tl>=VR){ed.tl=ed.cr-VR+1;if(ed.tl<0)ed.tl=0;}
    ed.dirty=1;
}
static void save(void) {
    int ri=0;ed.nd->content[0]=0;
    for(int i=0;i<ed.lc&&ri<MAX_CONTENT-6;i++){
        int ll=strlen(ed.l[i]);if(ll>MAX_CONTENT-6-ri)ll=MAX_CONTENT-6-ri;
        memcpy(ed.nd->content+ri,ed.l[i],ll);ri+=ll;
        if(ri<MAX_CONTENT-1){ed.nd->content[ri]='\n';ri++;}
    }ed.nd->content[ri]=0;
    memset(ed.nd->content+ri,0,MAX_CONTENT-ri);
    ed.dirty=0;
}
static int qprompt(void) {
    uint16_t *vb=(uint16_t*)0xB8000;uint8_t tc=COLOR_WHITE|(COLOR_BLUE<<4);
    int i=24*80;for(int x=0;x<80;x++)vb[i+x]=((uint16_t)' ')|((uint16_t)tc<<8);
    char *m=" tau - Save modified buffer? (Y/N/C)  ";i=24*80;
    for(int x=0;m[x];x++)vb[i+x]=(uint16_t)m[x]|((uint16_t)tc<<8);
    while(1){char c;if(!keyboard_getchar(&c))continue;
        if(c=='y'||c=='Y')return 1;
        if(c=='n'||c=='N')return 0;
        if(c=='c'||c=='C'||c==27)return -1;}
}
static int lpw(int cnt) { int t=cnt,n=1;while(t>=10){n++;t/=10;}return n+1; }
static int chw(int l, int *cpos, int *cnk) {
    char *ln=ed.l[l];int llen=strlen(ln),pos=0,ch=0;
    if(cpos)*cpos=0;if(cnk)*cnk=0;
    if(llen==0)return 1;
    while(1){
        int lm=(ch==0)?((ed.lnv)?lpw(ed.lc)+1:0):((ed.lnv)?lpw(ed.lc)+2:2);
        int av=80-lm;
        if(llen-pos<=av){if(cpos)*cpos=pos;if(cnk)*cnk=ch;return 1;}
        int end=pos+av,wp=end;
        for(int j=end-1;j>pos;j--)if(ln[j]==' '){wp=j+1;break;}
        if(wp<=pos)wp=end;
        if(cpos&&ed.cr==l&&ed.cc<wp&&ed.cc>=pos){*cpos=pos;*cnk=ch;return 1;}
        pos=wp;ch++;
    }
}
static int cnkl(int l) {
    char *ln=ed.l[l];int llen=strlen(ln),pos=0,ch=0;
    if(llen==0)return 1;
    while(1){
        int lm=(ch==0)?((ed.lnv)?lpw(ed.lc)+1:0):((ed.lnv)?lpw(ed.lc)+2:2);
        int av=80-lm;
        if(llen-pos<=av)return ch+1;
        int end=pos+av,wp=end;
        for(int j=end-1;j>pos;j--)if(ln[j]==' '){wp=j+1;break;}
        if(wp<=pos)wp=end;
        pos=wp;ch++;
    }
}
static void er(void) {
    uint16_t fb[2000];uint8_t tc=COLOR_WHITE|(COLOR_BLUE<<4);
    int i=0;for(int x=0;x<80;x++)fb[i++]=((uint16_t)' ')|((uint16_t)tc<<8);
    i=0;char*p=" tau - ";while(*p)fb[i++]=(uint16_t)*p++|((uint16_t)tc<<8);
    p=ed.fn;while(*p)fb[i++]=(uint16_t)*p++|((uint16_t)tc<<8);
    if(ed.dirty){p=" [Modified]";while(*p)fb[i++]=(uint16_t)*p++|((uint16_t)tc<<8);}
    if(ed.sel){p=" [SEL]";while(*p)fb[i++]=(uint16_t)*p++|((uint16_t)tc<<8);}
    p="  -- NORMAL --  ";while(*p)fb[i++]=(uint16_t)*p++|((uint16_t)tc<<8);
    uint8_t nc=COLOR_LIGHT_GREY|(COLOR_BLACK<<4);
    uint8_t hc=COLOR_BLACK|(COLOR_LIGHT_GREY<<4);
    for(int y=0;y<VR;y++){int b=(1+y)*80;for(int x=0;x<80;x++)fb[b+x]=((uint16_t)' ')|((uint16_t)nc<<8);}
    int sa=0,sb=0,sca=0,scb=0,has_sel=ed.sel;
    if(has_sel){
        if(ed.cr<ed.sl||(ed.cr==ed.sl&&ed.cc<ed.sc)){sa=ed.cr;sca=ed.cc;sb=ed.sl;scb=ed.sc;}
        else{sa=ed.sl;sca=ed.sc;sb=ed.cr;scb=ed.cc;}
    }
    cdr=-1;cdc=-1;int dr=0,li=ed.tl,pww=lpw(ed.lc);
    while(li<ed.lc&&dr<VR){
        char *ln=ed.l[li];int llen=strlen(ln),pos=0,ch=0;
        while(pos<llen&&dr<VR){
            int b=(1+dr)*80,lm;
            if(ch==0){
                if(ed.lnv){lm=pww+1;char num[8];itoa(li+1,num,10);int nl=strlen(num);
                    for(int x=0;x<lm;x++){
                        if(x<lm-nl-1)fb[b+x]=((uint16_t)' ')|((uint16_t)nc<<8);
                        else if(x<lm-1)fb[b+x]=((uint16_t)num[x-(lm-nl-1)])|((uint16_t)nc<<8);
                        else fb[b+x]=((uint16_t)' ')|((uint16_t)nc<<8);
                    }
                }else lm=0;
            }else{
                if(ed.lnv){lm=pww+2;
                    for(int x=0;x<lm-2;x++)fb[b+x]=((uint16_t)' ')|((uint16_t)nc<<8);
                    fb[b+lm-2]=((uint16_t)'^')|((uint16_t)nc<<8);
                    fb[b+lm-1]=((uint16_t)' ')|((uint16_t)nc<<8);
                }else{lm=2;fb[b]=((uint16_t)'^')|((uint16_t)nc<<8);fb[b+1]=((uint16_t)' ')|((uint16_t)nc<<8);}
            }
            int av=80-lm,rm=llen-pos,cl=rm;
            if(rm>av){int end=pos+av;cl=av;
                for(int j=end-1;j>pos;j--)if(ln[j]==' '){cl=j-pos+1;break;}}
            for(int x=0;x<cl;x++){
                uint8_t col=nc;
                if(has_sel){
                    int cx=pos+x;
                    if((li>sa&&li<sb)||(li==sa&&cx>=sca)||(li==sb&&cx<scb))col=hc;
                }
                fb[b+lm+x]=((uint16_t)ln[pos+x])|((uint16_t)col<<8);
            }
            if(li==ed.cr&&ed.cc>=pos&&(ed.cc<pos+cl||(ed.cc>=pos+cl&&rm<=av))){
                cdr=1+dr;cdc=lm+(ed.cc<pos+cl?ed.cc-pos:cl);}
            dr++;pos+=cl;if(cl>0&&pos>=llen)break;if(cl==0)break;ch++;
        }li++;
    }
    while(dr<VR){int b=(1+dr)*80;for(int x=0;x<80;x++)fb[b+x]=((uint16_t)' ')|((uint16_t)nc<<8);dr++;}
    i=24*80;for(int x=0;x<80;x++)fb[i++]=((uint16_t)' ')|((uint16_t)tc<<8);
    i=24*80;p=" Ln ";while(*p)fb[i++]=(uint16_t)*p++|((uint16_t)tc<<8);
    char bf[16];itoa(ed.cr+1,bf,10);p=bf;while(*p)fb[i++]=(uint16_t)*p++|((uint16_t)tc<<8);
    p=", Col ";while(*p)fb[i++]=(uint16_t)*p++|((uint16_t)tc<<8);
    itoa(ed.cc+1,bf,10);p=bf;while(*p)fb[i++]=(uint16_t)*p++|((uint16_t)tc<<8);
    if(cdr<0)cdr=VR+1;if(cdc<0)cdc=0;
    memcpy((uint16_t*)0xB8000,fb,sizeof(fb));
    if(cdr>=25)cdr=24;if(cdc>=80)cdc=79;
    vga_set_cursor(cdr,cdc);
    if(serial_is_present()){
        serial_write("\033[2J\033[H");
        serial_write(" tau - ");serial_write(ed.fn);
        serial_write("  -- NORMAL --  \r\n");
        for(int y=0;y<ed.lc&&y<24;y++){serial_write(ed.l[y]);serial_write("\r\n");}
        serial_write(" Ln ");itoa(ed.cr+1,bf,10);serial_write(bf);
        serial_write(", Col ");itoa(ed.cc+1,bf,10);serial_write(bf);
        serial_write("  \r\n");
    }
}
static char es_empty[1];
static char *es(const char *s) {
    char *c=(char*)malloc(EC+1);if(!c){char*e=(char*)malloc(1);if(e)e[0]=0;return e?e:es_empty;}
    strncpy(c,s,EC);c[EC]=0;return c;
}
void editor_open(fs_node_t *cwd, const char *filename) {
    uint16_t *vb=(uint16_t*)0xB8000;const char*ts="[tau] OK";
    for(int i=0;ts[i];i++)vb[i]=(uint16_t)ts[i]|((uint16_t)0x0F<<8);
    char p[MAX_PATH];fs_to_absolute(p,cwd,filename);
    fs_node_t *nd=fs_resolve(p,cwd);
    if(!nd){
        int ls=-1;for(int i=0;p[i];i++)if(p[i]=='/')ls=i;
        fs_node_t *par;char n[64];
        if(ls<=0){par=fs_get_root();strcpy(n,p+1);}
        else{char dp[MAX_PATH];strncpy(dp,p,ls);dp[ls]=0;par=fs_resolve(dp,cwd);strcpy(n,p+ls+1);}
        if(!par||!n[0]){vga_writeln("tau: bad path");return;}
        if(fs_find(par,n)){vga_writeln("tau: cannot create (exists)");return;}
        nd=fs_create_file(par,n);if(!nd){vga_writeln("tau: cannot create");return;}
    }
    if(nd->type!=FT_FILE){vga_writeln("tau: not a file");return;}
    ed.nd=nd;strncpy(ed.fn,nd->name,63);ed.fn[63]=0;
    ed.lc=0;ed.cr=0;ed.cc=0;ed.tl=0;ed.dirty=0;ed.lnv=0;ed.sel=0;ed.clip[0]=0;
    const char *ct=nd->content;char lb[EC+1];int ci=0;
    for(int i=0;;i++){
        if(ct[i]=='\n'||ct[i]==0){lb[ci]=0;ed.l[ed.lc++]=es(lb);ci=0;if(ct[i]==0||ed.lc>=EL)break;}
        else if(ci<EC)lb[ci++]=ct[i];
    }
    if(ed.lc==0){ed.l[0]=es("");ed.lc=1;}
    memcpy(sv,(uint16_t*)0xB8000,sizeof(sv));
    if(serial_is_present()){
        serial_write("\033[2J\033[H");
        serial_write(" tau - ");serial_write(ed.fn);
        serial_write("  -- NORMAL --  \r\n");
    }
    int run=1,rp=1;
    while(run){
        if(rp){er();rp=0;}
        char c;if(!keyboard_getchar(&c))continue;
        rp=1;int mod=0;
        if(c==17){
            if(ed.dirty){
                int r=qprompt();
                if(r==1){save();run=0;}
                else if(r==0){ed.dirty=0;run=0;}
                else rp=1;
            }else run=0;continue;
        }
        if(c==19){save();continue;}
        if(c==2){ed.lnv=!ed.lnv;continue;}
        if(c==3&&ed.sel){copy();continue;}
        if(c==22){paste();continue;}
        if(c==KEY_SLEFT||c==KEY_SRIGHT||c==KEY_SUP||c==KEY_SDOWN){
            if(!ed.sel){ed.sl=ed.cr;ed.sc=ed.cc;ed.sel=1;}
            if(c==KEY_SLEFT){if(ed.cc>0)ed.cc--;}
            else if(c==KEY_SRIGHT){int l=strlen(ed.l[ed.cr]);if(ed.cc<l)ed.cc++;}
            else if(c==KEY_SUP){if(ed.cr>0){ed.cr--;if(ed.cr<ed.tl)ed.tl=ed.cr;
                int l=strlen(ed.l[ed.cr]);if(ed.cc>l)ed.cc=l;}}
            else if(c==KEY_SDOWN){if(ed.cr<ed.lc-1){ed.cr++;if(ed.cr-ed.tl>=VR)ed.tl++;}
                int l=strlen(ed.l[ed.cr]);if(ed.cc>l)ed.cc=l;}
            continue;
        }
        if(ed.sel&&c!=3)ed.sel=0;
        if(c==KEY_DOWN){if(ed.cr<ed.lc-1){ed.cr++;if(ed.cr-ed.tl>=VR)ed.tl++;}}
        else if(c==KEY_UP){if(ed.cr>0){ed.cr--;if(ed.cr<ed.tl)ed.tl=ed.cr;}}
        else if(c==KEY_LEFT){if(ed.cc>0)ed.cc--;}
        else if(c==KEY_RIGHT){int l=strlen(ed.l[ed.cr]);if(ed.cc<l)ed.cc++;}
        else if(c==KEY_WORD_LEFT){wl();}
        else if(c==KEY_WORD_RIGHT){wr();}
        else if(c==KEY_WORD_DELETE){
            if(ed.cc>0){char*ln=ed.l[ed.cr];int s=ed.cc-1;while(s>0&&!wc(ln[s]))s--;while(s>0&&wc(ln[s-1]))s--;
                int ln2=ed.cc-s;for(int i=s;i+ln2<=EC;i++)ln[i]=ln[i+ln2];ed.cc=s;mod=1;}
            else if(ed.cr>0){int pl=strlen(ed.l[ed.cr-1]);int scl=strlen(ed.l[ed.cr]);int av=EC-pl;if(scl>av)scl=av;memcpy(ed.l[ed.cr-1]+pl,ed.l[ed.cr],scl);ed.l[ed.cr-1][pl+scl]=0;
                free(ed.l[ed.cr]);for(int i=ed.cr;i<ed.lc-1;i++)ed.l[i]=ed.l[i+1];ed.lc--;ed.cr--;ed.cc=pl;mod=1;}
        }
        else if(c=='\n'||c=='\r'){
            char*ln=ed.l[ed.cr];int l=strlen(ln);char*right=es(ln+ed.cc);
            ln[ed.cc]=0;
            if(ed.lc<EL-1){char*nl=es(ln);free(ed.l[ed.cr]);for(int i=ed.lc;i>ed.cr+1;i--)ed.l[i]=ed.l[i-1];
            ed.l[ed.cr]=nl;ed.l[ed.cr+1]=right;ed.lc++;ed.cr++;ed.cc=0;mod=1;
            if(ed.cr-ed.tl>=VR)ed.tl++;}
        }
        else if(c=='\b'){
            if(ed.cc>0){int l=strlen(ed.l[ed.cr]);for(int i=ed.cc-1;i<l;i++)ed.l[ed.cr][i]=ed.l[ed.cr][i+1];ed.cc--;mod=1;}
            else if(ed.cr>0){int pl=strlen(ed.l[ed.cr-1]);int scl=strlen(ed.l[ed.cr]);int av=EC-pl;if(scl>av)scl=av;memcpy(ed.l[ed.cr-1]+pl,ed.l[ed.cr],scl);ed.l[ed.cr-1][pl+scl]=0;
                free(ed.l[ed.cr]);for(int i=ed.cr;i<ed.lc-1;i++)ed.l[i]=ed.l[i+1];ed.lc--;ed.cr--;ed.cc=pl;mod=1;}
        }
        else if(c=='\t'){char*ln=ed.l[ed.cr];int l=strlen(ln);if(l+4<EC){for(int i=l;i>=ed.cc;i--)ln[i+4]=ln[i];
            ln[ed.cc]=' ';ln[ed.cc+1]=' ';ln[ed.cc+2]=' ';ln[ed.cc+3]=' ';ed.cc+=4;mod=1;}}
        else if(c>=32&&c<127){char*ln=ed.l[ed.cr];int l=strlen(ln);
            if(ed.cc>l)ed.cc=l;
            if(l<EC-1){for(int i=l+1;i>ed.cc;i--)ln[i]=ln[i-1];
            ln[ed.cc]=c;ed.cc++;mod=1;}}
        if(mod)ed.dirty=1;
        if(ed.cc<0)ed.cc=0;
    }
    if(ed.dirty) save();
    for(int i=0;i<ed.lc;i++)free(ed.l[i]);
    memcpy((uint16_t*)0xB8000,sv,sizeof(sv));
    vga_set_cursor(24,0);vga_set_fg(COLOR_LIGHT_GREY);vga_set_bg(COLOR_BLACK);
    vga_writeln("tau: saved");
}
