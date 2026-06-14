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
typedef struct { char *l[EL]; int lc, cr, cc, tl, dirty; char fn[64]; fs_node_t *nd; } ed_t;
static ed_t ed;
static uint16_t sv[2000];
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
static void vs(void) {
}
static void er(void) {
    uint16_t fb[2000];
    uint8_t tc=COLOR_WHITE|(COLOR_BLUE<<4);
    int i=0;for(int x=0;x<80;x++)fb[i++]=((uint16_t)' ')|((uint16_t)tc<<8);
    i=0;char*p=" tau - ";while(*p)fb[i++]=(uint16_t)*p++|((uint16_t)tc<<8);
    p=ed.fn;while(*p)fb[i++]=(uint16_t)*p++|((uint16_t)tc<<8);
    if(ed.dirty){p=" [Modified]";while(*p)fb[i++]=(uint16_t)*p++|((uint16_t)tc<<8);}
    p="  -- NORMAL --  ";while(*p)fb[i++]=(uint16_t)*p++|((uint16_t)tc<<8);
    uint8_t nc=COLOR_LIGHT_GREY|(COLOR_BLACK<<4);
    for(int y=0;y<VR;y++){
        int li=ed.tl+y,b=(1+y)*80;
        uint8_t lc=(li==ed.cr)?(COLOR_WHITE|(COLOR_BLACK<<4)):nc;
        for(int x=0;x<80;x++)fb[b+x]=((uint16_t)' ')|((uint16_t)lc<<8);
        if(li>=ed.lc)continue;
        int j=b;fb[j++]=((uint16_t)' ')|((uint16_t)lc<<8);fb[j++]=((uint16_t)' ')|((uint16_t)lc<<8);
        char*ln=ed.l[li];int cl=strlen(ln);int d=cl<78?cl:78;
        for(int k=0;k<d;k++)fb[j++]=(uint16_t)ln[k]|((uint16_t)lc<<8);
    }
    i=24*80;for(int x=0;x<80;x++)fb[i++]=((uint16_t)' ')|((uint16_t)tc<<8);
    i=24*80;p=" Ln ";while(*p)fb[i++]=(uint16_t)*p++|((uint16_t)tc<<8);
    char bf[16];itoa(ed.cr+1,bf,10);p=bf;while(*p)fb[i++]=(uint16_t)*p++|((uint16_t)tc<<8);
    p=", Col ";while(*p)fb[i++]=(uint16_t)*p++|((uint16_t)tc<<8);
    itoa(ed.cc+1,bf,10);p=bf;while(*p)fb[i++]=(uint16_t)*p++|((uint16_t)tc<<8);
    vs();memcpy((uint16_t*)0xB8000,fb,sizeof(fb));
    vga_set_cursor(1+ed.cr-ed.tl,2+ed.cc);
    serial_write("\033[H");
    serial_write(" tau - ");serial_write(ed.fn);
    if(ed.dirty)serial_write(" [Modified]");
    serial_write("  -- NORMAL --  \r\n");
    for(int y=0;y<VR;y++){
        int li=ed.tl+y;
        if(li>=ed.lc){serial_write("~\r\n");continue;}
        serial_write("  ");serial_write(ed.l[li]);serial_write("\r\n");
    }
    char bf[16];
    serial_write(" Ln ");itoa(ed.cr+1,bf,10);serial_write(bf);
    serial_write(", Col ");itoa(ed.cc+1,bf,10);serial_write(bf);
    serial_write("  \r\n");
}
static char *es(const char *s) {
    char *c=(char*)malloc(EC+1);if(!c){char*e=(char*)malloc(1);if(e)e[0]=0;return e?e:(char*)"";}
    strncpy(c,s,EC);c[EC]=0;return c;
}
void editor_open(fs_node_t *cwd, const char *filename) {
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
    ed.lc=0;ed.cr=0;ed.cc=0;ed.tl=0;ed.dirty=0;
    const char *ct=nd->content;char lb[EC+1];int ci=0;
    for(int i=0;;i++){
        if(ct[i]=='\n'||ct[i]==0){lb[ci]=0;ed.l[ed.lc++]=es(lb);ci=0;if(ct[i]==0||ed.lc>=EL)break;}
        else if(ci<EC)lb[ci++]=ct[i];
    }
    if(ed.lc==0){ed.l[0]=es("");ed.lc=1;}
    memcpy(sv,(uint16_t*)0xB8000,sizeof(sv));
    int run=1,rp=1;
    while(run){
        if(rp){er();rp=0;}
        char c;if(!keyboard_getchar(&c))continue;
        if(c==KEY_ESC){run=0;continue;}
        rp=1;int mod=0;
        if(c==KEY_DOWN){if(ed.cr<ed.lc-1){ed.cr++;if(ed.cr-ed.tl>=VR)ed.tl++;}}
        else if(c==KEY_UP){if(ed.cr>0){ed.cr--;if(ed.cr<ed.tl)ed.tl=ed.cr;}}
        else if(c==KEY_LEFT){if(ed.cc>0)ed.cc--;}
        else if(c==KEY_RIGHT){int l=strlen(ed.l[ed.cr]);if(ed.cc<l)ed.cc++;}
        else if(c==KEY_WORD_LEFT){wl();}
        else if(c==KEY_WORD_RIGHT){wr();}
        else if(c==KEY_WORD_DELETE){
            if(ed.cc>0){char*ln=ed.l[ed.cr];int s=ed.cc-1;while(s>0&&!wc(ln[s]))s--;while(s>0&&wc(ln[s-1]))s--;
                int ln2=ed.cc-s;for(int i=s;i+ln2<=EC;i++)ln[i]=ln[i+ln2];ed.cc=s;mod=1;}
            else if(ed.cr>0){int pl=strlen(ed.l[ed.cr-1]);strcat(ed.l[ed.cr-1],ed.l[ed.cr]);
                for(int i=ed.cr;i<ed.lc-1;i++)ed.l[i]=ed.l[i+1];ed.lc--;ed.cr--;ed.cc=pl;mod=1;}
        }
        else if(c=='\n'||c=='\r'){
            char*nl=es("");for(int i=ed.lc;i>ed.cr+1;i--)ed.l[i]=ed.l[i-1];
            ed.l[ed.cr+1]=nl;if(ed.lc<EL-1)ed.lc++;ed.cr++;ed.cc=0;mod=1;
            if(ed.cr-ed.tl>=VR)ed.tl++;
        }
        else if(c=='\b'){
            if(ed.cc>0){int l=strlen(ed.l[ed.cr]);for(int i=ed.cc-1;i<l;i++)ed.l[ed.cr][i]=ed.l[ed.cr][i+1];ed.cc--;mod=1;}
            else if(ed.cr>0){int pl=strlen(ed.l[ed.cr-1]);strcat(ed.l[ed.cr-1],ed.l[ed.cr]);
                for(int i=ed.cr;i<ed.lc-1;i++)ed.l[i]=ed.l[i+1];ed.lc--;ed.cr--;ed.cc=pl;mod=1;}
        }
        else if(c=='\t'){char*ln=ed.l[ed.cr];int l=strlen(ln);if(l+4<EC){for(int i=l+4;i>ed.cc;i--)ln[i]=ln[i-4];
            ln[ed.cc]=' ';ln[ed.cc+1]=' ';ln[ed.cc+2]=' ';ln[ed.cc+3]=' ';ed.cc+=4;mod=1;}}
        else if(c>=32&&c<127){char*ln=ed.l[ed.cr];int l=strlen(ln);if(l<EC-1){for(int i=l+1;i>ed.cc;i--)ln[i]=ln[i-1];
            ln[ed.cc]=c;ed.cc++;mod=1;}}
        if(mod)ed.dirty=1;
    }
    int ri=0;ed.nd->content[0]=0;
    for(int i=0;i<ed.lc&&ri<4090;i++){
        int ll=strlen(ed.l[i]);if(ll>4090-ri)ll=4090-ri;
        memcpy(ed.nd->content+ri,ed.l[i],ll);ri+=ll;
        if(ri<4095){ed.nd->content[ri]='\n';ri++;}
    }
    ed.nd->content[ri]=0;
    memcpy((uint16_t*)0xB8000,sv,sizeof(sv));
    vga_set_cursor(24,0);vga_set_fg(COLOR_LIGHT_GREY);vga_set_bg(COLOR_BLACK);
    vga_writeln("tau: saved");
}
