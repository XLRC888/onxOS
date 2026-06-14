#include "fs.h"
#include "string.h"
#include "memory.h"
#include "port.h"
#include "serial.h"
#define FS_MAGIC 0x58464E4F
#define NODE_DISK_SECTORS 9
typedef struct {
    uint32_t magic;
    uint32_t node_count;
    uint32_t root_index;
    uint8_t pad[500];
} __attribute__((packed)) superblock_t;
typedef struct {
    char name[64];
    uint32_t type;
    uint32_t content_size;
    int32_t parent_index;
    uint32_t child_count;
    uint32_t child_indices[32];
    uint8_t pad[132];
} __attribute__((packed)) node_meta_t;
static fs_node_t root_node;
static int ata_disk = 0;
static uint16_t ata_base = 0;
static uint16_t ata_base2 = 0;
static uint32_t data_lba = 0;
static int boot_media = 0;
static uint32_t pci_read(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off) {
    uint32_t addr = (uint32_t)0x80000000 | ((uint32_t)bus << 16) | ((uint32_t)dev << 11) | ((uint32_t)func << 8) | (off & 0xFC);
    outl(0xCF8, addr);
    return inl(0xCFC);
}
static int ata_init(void) {
    uint16_t b1 = 0, b2 = 0;
    for (int d = 0; d < 32; d++) for (int f = 0; f < 8; f++) {
        uint32_t id = pci_read(0, d, f, 0);
        if (id == 0xFFFFFFFF) { if (f == 0) break; continue; }
        uint16_t cc = (uint16_t)(pci_read(0, d, f, 8) >> 16);
        if (cc == 0x0101 || cc == 0x0100 || cc == 0x0106 || cc == 0x0104) {
            uint32_t cmd = pci_read(0, d, f, 4);
            cmd |= 0x07;
            outl(0xCF8, (uint32_t)(0x80000000 | (d << 11) | (f << 8) | 4));
            outl(0xCFC, cmd);
            uint32_t bar0 = pci_read(0, d, f, 0x10);
            uint32_t bar2 = pci_read(0, d, f, 0x18);
            if ((bar0 & 1) && (bar0 & 0xFFF8)) b1 = (uint16_t)(bar0 & 0xFFF8);
            if ((bar2 & 1) && (bar2 & 0xFFF8)) b2 = (uint16_t)(bar2 & 0xFFF8);
            break;
        }
    }
    if (!b1) b1 = 0x1F0;
    if (!b2) b2 = 0x170;
    uint8_t st1 = inb(b1 + 7);
    if (st1 != 0xFF && st1 != 0) {
        int clean = 0;
        for (int i = 0; i < 100000; i++) { st1 = inb(b1 + 7); if (!(st1 & 0x80)) { clean = 1; break; } }
        if (!clean || st1 == 0xFF || st1 == 0) st1 = 0xFF;
    }
    uint8_t st2 = inb(b2 + 7);
    if (st2 != 0xFF && st2 != 0) {
        int clean = 0;
        for (int i = 0; i < 100000; i++) { st2 = inb(b2 + 7); if (!(st2 & 0x80)) { clean = 1; break; } }
        if (!clean || st2 == 0xFF || st2 == 0) st2 = 0xFF;
    }
    if (!(st1 == 0xFF || st1 == 0)) ata_base = b1;
    if (!(st2 == 0xFF || st2 == 0)) ata_base2 = b2;
    ata_disk = ata_base || ata_base2;
    return ata_disk;
}
static int ata_poll(uint16_t base) {
    for (int i = 0; i < 500000; i++) {
        uint8_t st = inb(base + 7);
        if (st & 1) return 0;
        if (!(st & 0x80) && (st & 8)) return 1;
        __asm__ volatile ("pause");
    }
    return 0;
}
static int ata_flush(uint16_t base) {
    outb(base + 7, 0xE7);
    for (int i = 0; i < 500000; i++) {
        uint8_t st = inb(base + 7);
        if (!(st & 0x80) && (st & 0x40)) return 1;
        if (st & 1) return 0;
    }
    return 0;
}
static int ata_rw(int wr, uint32_t lba, uint8_t cnt, void *buf, uint16_t base) {
    if (!base) return 0;
    outb(base + 6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(base + 2, cnt);
    outb(base + 3, lba & 0xFF);
    outb(base + 4, (lba >> 8) & 0xFF);
    outb(base + 5, (lba >> 16) & 0xFF);
    outb(base + 7, wr ? 0x30 : 0x20);
    uint16_t *w = (uint16_t *)buf;
    for (int s = 0; s < cnt; s++) {
        if (!ata_poll(base)) return 0;
        if (wr) { for (int i = 0; i < 256; i++) outw(base, w[i]); w += 256; }
        else { for (int i = 0; i < 256; i++) w[i] = inw(base); w += 256; }
    }
    if (wr && !ata_flush(base)) return 0;
    return 1;
}
static int ahci_ok = 0;
static int ahci_rw(int wr, uint32_t lba, uint8_t cnt, void *buf);
static int ata_rd(uint32_t lba, uint8_t cnt, void *b) { uint32_t dl = data_lba; return (ahci_ok && ahci_rw(0, lba + dl, cnt, b)) || (ata_base && ata_rw(0, lba + dl, cnt, b, ata_base)) || (ata_base2 && ata_rw(0, lba + dl, cnt, b, ata_base2)) || ata_rw(0, lba + dl, cnt, b, 0x1F0) || ata_rw(0, lba + dl, cnt, b, 0x170); }
static int ata_wr(uint32_t lba, uint8_t cnt, const void *b) { uint32_t dl = data_lba; return (ahci_ok && ahci_rw(1, lba + dl, cnt, (void *)b)) || (ata_base && ata_rw(1, lba + dl, cnt, (void *)b, ata_base)) || (ata_base2 && ata_rw(1, lba + dl, cnt, (void *)b, ata_base2)) || ata_rw(1, lba + dl, cnt, (void *)b, 0x1F0) || ata_rw(1, lba + dl, cnt, (void *)b, 0x170); }
static char ahci_pool[4096];
static uint32_t ahci_cl_pa, ahci_fis_pa, ahci_ct_pa;
static volatile void *ahci_abar = 0;
static int ahci_port = -1;

#define A_POR_CMD 0x18
#define A_POR_TFD 0x20
#define A_POR_SST 0x28
#define A_POR_SER 0x30
#define A_POR_CI 0x38
#define A_POR_SF 0x3C

static void ahci_setup(void) {
    uint32_t b = (uint32_t)ahci_pool;
    ahci_cl_pa = (b + 1023) & ~1023;
    ahci_fis_pa = (ahci_cl_pa + 1024 + 255) & ~255;
    ahci_ct_pa = (ahci_fis_pa + 256 + 127) & ~127;
}

static uint32_t apr(int off) {
    return *(volatile uint32_t *)((uint32_t)ahci_abar + 0x100 + ahci_port * 0x80 + off);
}

static void apw(int off, uint32_t v) {
    *(volatile uint32_t *)((uint32_t)ahci_abar + 0x100 + ahci_port * 0x80 + off) = v;
}

static int ahci_wait(volatile uint32_t *r, uint32_t clr, uint32_t set) {
    for (int i = 0; i < 2000000; i++) { uint32_t v = *r; if (clr && !(v & clr)) return 1; if (set && (v & set)) return 1; }
    return 0;
}

static int ahci_rw(int wr, uint32_t lba, uint8_t cnt, void *buf) {
    if (ahci_port < 0) return 0;
    uint8_t *cfis = (uint8_t *)ahci_ct_pa;
    for (int i = 0; i < 64; i++) cfis[i] = 0;
    cfis[0] = 0x27; cfis[1] = 0x80;
    cfis[2] = wr ? 0xCA : 0xC8;
    cfis[4] = (uint8_t)(lba);
    cfis[5] = (uint8_t)(lba >> 8);
    cfis[6] = (uint8_t)(lba >> 16);
    cfis[7] = 0xE0 | (uint8_t)((lba >> 24) & 0x0F);
    cfis[12] = cnt;
    volatile uint32_t *ch = (volatile uint32_t *)ahci_cl_pa;
    ch[0] = 5 | (wr ? (1 << 6) : 0) | (1 << 16);
    ch[1] = 0;
    ch[2] = ahci_ct_pa;
    ch[3] = 0;
    for (int i = 4; i < 8; i++) ch[i] = 0;
    volatile uint32_t *prdt = (volatile uint32_t *)(ahci_ct_pa + 0x80);
    prdt[0] = (uint32_t)buf;
    prdt[1] = 0;
    prdt[2] = ((uint32_t)cnt * 512 - 1) | 0x80000000;
    prdt[3] = 0;
    apw(A_POR_CI, 1);
    uint32_t *ci_reg = (uint32_t *)((uint32_t)ahci_abar + 0x100 + ahci_port * 0x80 + A_POR_CI);
    int to = 5000000;
    while ((*ci_reg & 1) && to--) { __asm__ volatile ("pause"); }
    if (!to) return 0;
    if (apr(A_POR_TFD) & 0xFF) return 0;
    return 1;
}

static int ahci_init(void) {
    int found = 0; uint32_t bar5 = 0; int bdf = 0;
    for (int d = 0; d < 32 && !found; d++) for (int f = 0; f < 8 && !found; f++) {
        uint32_t id = pci_read(0, d, f, 0);
        if (id == 0xFFFFFFFF) { if (f == 0) break; continue; }
        uint32_t cc = pci_read(0, d, f, 8);
        uint8_t cls = (cc >> 24) & 0xFF;
        uint8_t scl = (cc >> 16) & 0xFF;
        if ((cls == 0x01 && scl == 0x06) || (cls == 0x01 && scl == 0x04)) {
            bar5 = pci_read(0, d, f, 0x24);
            if (bar5 & ~0x0F) {
                uint32_t cmd = pci_read(0, d, f, 4);
                cmd |= 0x07;
                outl(0xCF8, (0x80000000 | (d << 11) | (f << 8) | 4));
                outl(0xCFC, cmd);
                found = 1;
            }
        }
    }
    if (!found) return 0;
    ahci_abar = (volatile void *)(uint32_t)(bar5 & ~0x0F);
    ahci_setup();
    volatile uint32_t *ghc = (volatile uint32_t *)((uint32_t)ahci_abar + 0x04);
    while (*ghc & 1) { __asm__ volatile ("pause"); }
    *ghc |= 0x80000000;
    uint32_t pi = *(volatile uint32_t *)((uint32_t)ahci_abar + 0x0C);
    for (int p = 0; p < 32; p++) {
        if (!(pi & (1 << p))) continue;
        uint32_t ssts = *(volatile uint32_t *)((uint32_t)ahci_abar + 0x100 + p * 0x80 + A_POR_SST);
        if ((ssts & 0x0F) != 3) continue;
        if (!(ssts & 0xF0)) continue;
        ahci_port = p;
        volatile uint32_t *cmd = (volatile uint32_t *)((uint32_t)ahci_abar + 0x100 + p * 0x80 + A_POR_CMD);
        *cmd &= ~1;
        ahci_wait(cmd, 0x8000, 0);
        *cmd &= ~0x10;
        ahci_wait(cmd, 0x4000, 0);
        *(volatile uint32_t *)((uint32_t)ahci_abar + 0x100 + p * 0x80 + 0x00) = ahci_cl_pa;
        *(volatile uint32_t *)((uint32_t)ahci_abar + 0x100 + p * 0x80 + 0x04) = 0;
        *(volatile uint32_t *)((uint32_t)ahci_abar + 0x100 + p * 0x80 + 0x08) = ahci_fis_pa;
        *(volatile uint32_t *)((uint32_t)ahci_abar + 0x100 + p * 0x80 + 0x0C) = 0;
        *(volatile uint32_t *)((uint32_t)ahci_abar + 0x100 + p * 0x80 + 0x10) = 0xFFFFFFFF;
        *(volatile uint32_t *)((uint32_t)ahci_abar + 0x100 + p * 0x80 + A_POR_SER) = 0xFFFFFFFF;
        *cmd |= 0x10;
        ahci_wait(cmd, 0, 0x4000);
        *cmd |= 1;
        ahci_wait(cmd, 0, 0x8000);
        ahci_ok = 1;
        return 1;
    }
    return 0;
}

static uint32_t find_data_part(void) {
    uint8_t buf[512];
    int ok = 0;
    if (ahci_ok) ok = ahci_rw(0, 0, 1, buf);
    if (!ok && ata_base) ok = ata_rw(0, 0, 1, buf, ata_base);
    if (!ok && ata_base2) ok = ata_rw(0, 0, 1, buf, ata_base2);
    if (!ok) ok = ata_rw(0, 0, 1, buf, 0x1F0);
    if (!ok) ok = ata_rw(0, 0, 1, buf, 0x170);
    if (!ok) return 0;
    if (buf[510] != 0x55 || buf[511] != 0xAA) return 0;
    for (int i = 0; i < 4; i++) {
        uint8_t *e = buf + 446 + i * 16;
        if (e[4] == 0xDA) return *(uint32_t *)(e + 8);
    }
    return 0;
}

void fs_init(void) {
    root_node.name[0] = 0;
    root_node.type = FT_DIR;
    root_node.parent = &root_node;
    root_node.child_count = 0;
    root_node.content[0] = 0;
    ahci_init();
    serial_write("ahci ");serial_write(ahci_ok?"ok":"no");serial_write("\n");
    if (!ahci_ok) ata_init();
    serial_write("ata ");serial_write(ata_base?"ok":"no");serial_write(" ");serial_write(ata_base2?"ok":"no");serial_write("\n");
    data_lba = find_data_part();
    if (data_lba) { serial_write("part lba ");serial_write_dec(data_lba);serial_write("\n"); }
    else serial_write("no part\n");
}
int fs_ata_present(void) { return ata_disk || ahci_ok; }
fs_node_t *fs_get_root(void) { return &root_node; }
static void pn(char *out, const char *path) {
    char buf[MAX_PATH];
    strcpy(buf, path);
    char *parts[MAX_PATH];
    int np = 0;
    char *tok = buf;
    int in = 0;
    for (int i = 0; ; i++) {
        int end = (buf[i] == 0);
        if (buf[i] == '/' || end) {
            if (in) {
                buf[i] = 0;
                if (tok[0]=='.'&&tok[1]=='.'&&tok[2]==0) { if (np > 0) np--; }
                else if (tok[0]!='.'||tok[1]!=0) { parts[np++] = tok; }
                in = 0;
            }
            if (end) break;
        } else if (!in) { tok = &buf[i]; in = 1; }
    }
    out[0]='/'; int pos=1;
    for (int i=0;i<np;i++){if(pos>1)out[pos++]='/';int j=0;while(parts[i][j])out[pos++]=parts[i][j++];}
    out[pos]=0;if(pos==1)out[0]='/',out[1]=0;
}
static void bp(char *out, fs_node_t *node) {
    if (node->parent == node) { out[0]='/';out[1]=0;return; }
    bp(out, node->parent);
    int l = strlen(out);
    if (l>1||out[0]!='/'){out[l]='/';out[++l]=0;}
    strcpy(out+l, node->name);
}
void fs_to_absolute(char *out, fs_node_t *cwd, const char *rel) {
    char abs[MAX_PATH], exp[MAX_PATH];
    const char *p = rel;
    if (p[0]=='~'&&(p[1]=='/'||p[1]==0)){exp[0]='/';if(p[1]=='/')strcpy(exp+1,p+2);else exp[1]=0;p=exp;}
    if (p[0]=='/') { strcpy(abs, p); }
    else { bp(abs, cwd); if(*p){int l=strlen(abs);if(l>1||abs[0]!='/')abs[l++]='/';abs[l]=0;strcat(abs,p);} }
    pn(out, abs);
}
fs_node_t *fs_find(fs_node_t *dir, const char *name) {
    if (!dir || dir->type != FT_DIR) return 0;
    for (int i = 0; i < dir->child_count; i++)
        if (strcmp(dir->children[i]->name, name) == 0) return dir->children[i];
    return 0;
}
static fs_node_t *fn(const char *name, file_type_t t, fs_node_t *parent) {
    if (!parent||parent->type!=FT_DIR)return 0;
    if (parent->child_count>=MAX_CHILDREN||!name||!name[0]||strchr(name,'/')||fs_find(parent,name))return 0;
    fs_node_t *n=(fs_node_t*)malloc(sizeof(fs_node_t));
    if(!n)return 0;
    strncpy(n->name,name,MAX_NAME-1);n->name[MAX_NAME-1]=0;
    n->type=t;n->parent=parent;n->child_count=0;n->content[0]=0;
    parent->children[parent->child_count++]=n;
    return n;
}
fs_node_t *fs_create_dir(fs_node_t *p, const char *n) { return fn(n, FT_DIR, p); }
fs_node_t *fs_create_file(fs_node_t *p, const char *n) { return fn(n, FT_FILE, p); }
int fs_delete(fs_node_t *parent, const char *name) {
    if (!parent||parent->type!=FT_DIR)return 0;
    for(int i=0;i<parent->child_count;i++){
        if(strcmp(parent->children[i]->name,name)!=0)continue;
        if(parent->children[i]->type==FT_DIR&&parent->children[i]->child_count>0)return -1;
        free(parent->children[i]);
        for(int j=i;j<parent->child_count-1;j++)parent->children[j]=parent->children[j+1];
        parent->child_count--;return 1;
    }
    return 0;
}
int fs_is_child_of(fs_node_t *parent, fs_node_t *child) {
    fs_node_t *p = child;
    while (p != p->parent) { if (p == parent) return 1; p = p->parent; }
    return 0;
}
fs_node_t *fs_resolve(const char *path, fs_node_t *cwd) {
    char n[MAX_PATH];
    fs_to_absolute(n, cwd, path);
    fs_node_t *c = &root_node;
    if (strcmp(n, "/") == 0) return c;
    char b[MAX_PATH]; strcpy(b, n);
    char *part = b; int in = 0;
    for (int i = 0; ; i++) {
        int end = (b[i] == 0);
        if (b[i] == '/' || end) {
            if (in) { b[i]=0; c=fs_find(c,part); if(!c)return 0; in=0; }
            if (end) break;
        } else if (!in) { part = &b[i]; in = 1; }
    }
    return c;
}
static int gat(fs_node_t *nodes[], int max) {
    int c = 0; nodes[c++] = &root_node;
    for (int i = 0; i < c && c < max; i++)
        for (int j = 0; j < nodes[i]->child_count && c < max; j++)
            nodes[c++] = nodes[i]->children[j];
    return c;
}
static int fi(fs_node_t *nodes[], int count, fs_node_t *node) {
    for (int i = 0; i < count; i++) if (nodes[i] == node) return i;
    return -1;
}
static void spack(fs_node_t *nodes[], int count, uint8_t *buf, int i) {
    memset(buf, 0, ATA_SECTOR_SIZE * NODE_DISK_SECTORS);
    node_meta_t *m = (node_meta_t *)buf;
    strncpy(m->name, nodes[i]->name, 63); m->name[63] = 0;
    m->type = (uint32_t)nodes[i]->type;
    m->content_size = strlen(nodes[i]->content);
    m->parent_index = fi(nodes, count, nodes[i]->parent);
    m->child_count = nodes[i]->child_count;
    for (int j = 0; j < (int)nodes[i]->child_count; j++)
        m->child_indices[j] = fi(nodes, count, nodes[i]->children[j]);
    if (nodes[i]->type == FT_FILE && m->content_size > 0)
        memcpy(buf + ATA_SECTOR_SIZE, nodes[i]->content, m->content_size);
}

int fs_write_sectors(uint32_t lba, uint8_t cnt, const void *b) {
    return (ahci_ok && ahci_rw(1, lba, cnt, (void *)b)) || (ata_base && ata_rw(1, lba, cnt, (void *)b, ata_base)) || (ata_base2 && ata_rw(1, lba, cnt, (void *)b, ata_base2)) || ata_rw(1, lba, cnt, (void *)b, 0x1F0) || ata_rw(1, lba, cnt, (void *)b, 0x170);
}
int fs_read_sectors(uint32_t lba, uint8_t cnt, void *b) {
    return (ahci_ok && ahci_rw(0, lba, cnt, b)) || (ata_base && ata_rw(0, lba, cnt, b, ata_base)) || (ata_base2 && ata_rw(0, lba, cnt, b, ata_base2)) || ata_rw(0, lba, cnt, b, 0x1F0) || ata_rw(0, lba, cnt, b, 0x170);
}
void fs_set_data_lba(uint32_t lba) { data_lba = lba; }
uint32_t fs_get_data_lba(void) { return data_lba; }
void fs_set_boot_media(int v) { boot_media = v; }
int fs_get_boot_media(void) { return boot_media; }
int fs_present(void) { return ata_base || ata_base2 || ahci_ok; }

int fs_save_disk(void) {
    fs_node_t *nodes[MAX_FILES];
    int count = gat(nodes, MAX_FILES);
    superblock_t sb;
    memset(&sb, 0, sizeof(sb));
    sb.magic = FS_MAGIC; sb.node_count = count; sb.root_index = 0;
    uint8_t *buf = (uint8_t *)malloc(ATA_SECTOR_SIZE * NODE_DISK_SECTORS);
    if (!buf) { serial_write("fs_save: no buf\n"); return 0; }
    int ok = 0, td = ata_base || ata_base2 || ahci_ok;
    serial_write("fs_save: td=");serial_write(td?"y":"n");serial_write(" dlba=");serial_write_dec(data_lba);serial_write("\n");
    if (td) {
        ok = 1;
        if (data_lba == 0) {
            uint32_t probe = find_data_part();
            if (probe) { data_lba = probe; fs_set_data_lba(probe); }
        }
        if (data_lba == 0) {
            uint8_t mbr[512]; int got = ata_rd(0, 1, mbr);
            serial_write("fs_save: mbr got=");serial_write(got?"y":"n");serial_write("\n");
            if (got && mbr[510] == 0x55 && mbr[511] == 0xAA) {
                for (int p = 0; p < 4; p++) {
                    uint8_t *e = mbr + 446 + p * 16;
                    if (e[4] == 0) {
                        uint32_t dl = 128 + p * 2048;
                        e[0] = 0x80; e[4] = 0xDA;
                        e[8] = dl & 0xFF; e[9] = (dl >> 8) & 0xFF;
                        e[10] = (dl >> 16) & 0xFF; e[11] = (dl >> 24) & 0xFF;
                        e[12] = 0; e[13] = 8; e[14] = 0; e[15] = 0;
                        ata_wr(0, 1, mbr);
                        data_lba = dl; fs_set_data_lba(dl);
                        break;
                    }
                }
            } else if (got) {
                memset(mbr, 0, 512);
                mbr[446] = 0x80; mbr[450] = 0xDA;
                uint32_t dl = 128;
                mbr[454] = dl & 0xFF; mbr[455] = (dl >> 8) & 0xFF;
                mbr[456] = (dl >> 16) & 0xFF; mbr[457] = (dl >> 24) & 0xFF;
                mbr[458] = 0; mbr[459] = 8; mbr[460] = 0; mbr[461] = 0;
                mbr[510] = 0x55; mbr[511] = 0xAA;
                ata_wr(0, 1, mbr);
                data_lba = dl; fs_set_data_lba(dl);
            }
            if (data_lba == 0) { serial_write("fs_save: no dlba\n"); ok = 0; goto out; }
        }
        if (!ata_wr(0, 1, &sb)) { serial_write("fs_save: sb write fail\n"); ok = 0; goto out; }
        for (int i = 0; i < count; i++) {
            spack(nodes, count, buf, i);
            if (!ata_wr(1 + i * NODE_DISK_SECTORS, NODE_DISK_SECTORS, buf)) { serial_write("fs_save: node write fail\n"); ok = 0; goto out; }
        }
    } else { serial_write("fs_save: no disk hw\n"); }
out:
    free(buf);
    return ok;
}
int fs_seed_disk(void) { return fs_save_disk(); }
static int dld(sector_read_t rd) {
    superblock_t sb;
    if (!rd(0, 1, &sb) || sb.magic != FS_MAGIC) return 0;
    if (sb.node_count > (uint32_t)MAX_FILES || sb.node_count == 0) return 0;
    int count = (int)sb.node_count;
    if (count > MAX_FILES || count == 0) return 0;
    node_meta_t *metas = (node_meta_t *)malloc(sizeof(node_meta_t) * count);
    if (!metas) return 0;
    memset(&root_node, 0, sizeof(root_node));
    root_node.parent = &root_node;
    fs_node_t *loaded[MAX_FILES];
    uint8_t *buf = (uint8_t *)malloc(ATA_SECTOR_SIZE * NODE_DISK_SECTORS);
    if (!buf) { free(metas); return 0; }
    for (int i = 0; i < count; i++) {
        if (!rd(1 + i * NODE_DISK_SECTORS, NODE_DISK_SECTORS, buf)) { free(buf); free(metas); return 0; }
        memcpy(&metas[i], buf, sizeof(node_meta_t));
        if (i == 0) loaded[i] = &root_node;
        else { loaded[i] = (fs_node_t *)malloc(sizeof(fs_node_t)); if (!loaded[i]) { free(buf); free(metas); return 0; } memset(loaded[i], 0, sizeof(fs_node_t)); }
        strncpy(loaded[i]->name, metas[i].name, MAX_NAME - 1); loaded[i]->name[MAX_NAME - 1] = 0;
        loaded[i]->type = (file_type_t)metas[i].type;
        if (metas[i].type == FT_FILE && metas[i].content_size > 0) {
            uint32_t raw = metas[i].content_size;
            int sz = raw > (uint32_t)(MAX_CONTENT - 1) ? (MAX_CONTENT - 1) : (int)raw;
            memcpy(loaded[i]->content, buf + ATA_SECTOR_SIZE, sz); loaded[i]->content[sz] = 0;
        }
    }
    free(buf);
    for (int i = 0; i < count; i++) {
        loaded[i]->parent = (metas[i].parent_index >= 0 && metas[i].parent_index < count) ? loaded[metas[i].parent_index] : &root_node;
        loaded[i]->child_count = 0;
        for (int j = 0; j < (int)metas[i].child_count && j < MAX_CHILDREN; j++) {
            int ci = (int)metas[i].child_indices[j];
            if (ci >= 0 && ci < count) loaded[i]->children[loaded[i]->child_count++] = loaded[ci];
        }
    }
    free(metas);
    root_node.parent = &root_node;
    return 1;
}
int fs_load_disk(void) {
    return dld(ata_rd);
}
int fs_load_from_memory(void *data) {
    superblock_t sb;
    memcpy(&sb, data, sizeof(sb));
    if (sb.magic != FS_MAGIC) return 0;
    if (sb.node_count > (uint32_t)MAX_FILES || sb.node_count == 0) return 0;
    int count = (int)sb.node_count;
    if (count > MAX_FILES || count == 0) return 0;
    node_meta_t *metas = (node_meta_t *)malloc(sizeof(node_meta_t) * count);
    if (!metas) return 0;
    memset(&root_node, 0, sizeof(root_node));
    root_node.parent = &root_node;
    fs_node_t *loaded[MAX_FILES];
    for (int i = 0; i < count; i++) {
        uint32_t off = 512 + (uint32_t)i * NODE_DISK_SECTORS * 512;
        memcpy(&metas[i], (uint8_t *)data + off, sizeof(node_meta_t));
        if (i == 0) loaded[i] = &root_node;
        else { loaded[i] = (fs_node_t *)malloc(sizeof(fs_node_t)); if (!loaded[i]) { free(metas); return 0; } memset(loaded[i], 0, sizeof(fs_node_t)); }
        strncpy(loaded[i]->name, metas[i].name, MAX_NAME - 1); loaded[i]->name[MAX_NAME - 1] = 0;
        loaded[i]->type = (file_type_t)metas[i].type;
        if (metas[i].type == FT_FILE && metas[i].content_size > 0) {
            uint32_t raw = metas[i].content_size;
            int sz = raw > (uint32_t)(MAX_CONTENT - 1) ? (MAX_CONTENT - 1) : (int)raw;
            memcpy(loaded[i]->content, (uint8_t *)data + off + 512, sz); loaded[i]->content[sz] = 0;
        }
    }
    for (int i = 0; i < count; i++) {
        loaded[i]->parent = (metas[i].parent_index >= 0 && metas[i].parent_index < count) ? loaded[metas[i].parent_index] : &root_node;
        loaded[i]->child_count = 0;
        for (int j = 0; j < (int)metas[i].child_count && j < MAX_CHILDREN; j++) {
            int ci = (int)metas[i].child_indices[j];
            if (ci >= 0 && ci < count) loaded[i]->children[loaded[i]->child_count++] = loaded[ci];
        }
    }
    free(metas);
    root_node.parent = &root_node;
    return 1;
}
