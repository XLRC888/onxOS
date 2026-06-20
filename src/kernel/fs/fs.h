#ifndef FS_H
#define FS_H
#include "kernel.h"
#define MAX_FILES 128
#define MAX_NAME 64
#define MAX_PATH 256
#define MAX_CONTENT 4096
#define MAX_CHILDREN 32
#define ATA_SECTOR_SIZE 512
typedef enum { FT_DIR, FT_FILE } file_type_t;
typedef struct fs_node {
    char name[MAX_NAME];
    file_type_t type;
    char content[MAX_CONTENT];
    int content_size;
    struct fs_node *parent;
    struct fs_node *children[MAX_CHILDREN];
    int child_count;
} fs_node_t;
typedef int (*sector_read_t)(uint32_t lba, uint8_t count, void *buf);
void fs_init(void);
fs_node_t *fs_get_root(void);
fs_node_t *fs_create_dir(fs_node_t *parent, const char *name);
fs_node_t *fs_create_file(fs_node_t *parent, const char *name);
int fs_delete(fs_node_t *parent, const char *name);
fs_node_t *fs_find(fs_node_t *dir, const char *name);
fs_node_t *fs_resolve(const char *path, fs_node_t *cwd);
void fs_to_absolute(char *out, fs_node_t *cwd, const char *relative);
int fs_is_child_of(fs_node_t *parent, fs_node_t *child);
int fs_ata_present(void);
int fs_save_disk(void);
int fs_seed_disk(void);
int fs_load_disk(void);
int fs_load_from_memory(void *data);
int fs_write_sectors(uint32_t lba, uint8_t cnt, const void *buf);
int fs_read_sectors(uint32_t lba, uint8_t cnt, void *buf);
void fs_set_data_lba(uint32_t lba);
uint32_t fs_get_data_lba(void);
void fs_set_boot_media(int v);
int fs_get_boot_media(void);
int fs_present(void);
#endif
