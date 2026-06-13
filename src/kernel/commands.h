#ifndef COMMANDS_H
#define COMMANDS_H
#include "kernel.h"
#include "fs.h"
void cmd_help(void);
void cmd_clear(void);
void cmd_pwd(fs_node_t *cwd);
void cmd_ls(fs_node_t *cwd, const char *arg);
void cmd_cd(fs_node_t **cwd, const char *arg);
void cmd_mkdir(fs_node_t *cwd, const char *arg);
void cmd_touch(fs_node_t *cwd, const char *arg);
void cmd_rm(fs_node_t *cwd, const char *arg);
void cmd_rmdir(fs_node_t *cwd, const char *arg);
void cmd_cat(fs_node_t *cwd, const char *arg);
void cmd_echo(const char *arg);
void cmd_cp(fs_node_t *cwd, const char *arg);
void cmd_mv(fs_node_t *cwd, const char *arg);
void cmd_stat(fs_node_t *cwd, const char *arg);
void cmd_hexdump(fs_node_t *cwd, const char *arg);
void cmd_ver(void);
void cmd_reboot(void);
void cmd_tau(fs_node_t *cwd, const char *arg);
void cmd_tree(fs_node_t *cwd, const char *arg);
void cmd_cowsay(const char *arg);
void cmd_find(fs_node_t *cwd, const char *arg);
void cmd_grep(fs_node_t *cwd, const char *arg);
void cmd_setup(fs_node_t *cwd);
void cmd_head(fs_node_t *cwd, const char *arg);
void cmd_tail(fs_node_t *cwd, const char *arg);
void cmd_wc(fs_node_t *cwd, const char *arg);
void cmd_history(void);
void cmd_uname(void);
void cmd_df(void);
void cmd_du(fs_node_t *cwd, const char *arg);
void cmd_sort(fs_node_t *cwd, const char *arg);
void cmd_yes(const char *arg);
void cmd_sleep(const char *arg);
void cmd_seq(const char *arg);
void cmd_rev(fs_node_t *cwd, const char *arg);
void cmd_which(fs_node_t *cwd, const char *arg);
void cmd_tac(fs_node_t *cwd, const char *arg);
void cmd_base64(fs_node_t *cwd, const char *arg);
void cmd_uniq(fs_node_t *cwd, const char *arg);
#endif
