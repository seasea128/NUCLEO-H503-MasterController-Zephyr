#ifndef FILE_OP_H_
#define FILE_OP_H_
#include "zephyr/fs/fs.h"
#include <ff.h>

int file_op_open_file(struct fs_file_t *file, char *file_name, fs_mode_t flags);

int file_op_get_count_in_dir(char *path, int *file_count);

int file_op_mount_disk();

int file_op_close_file(struct fs_file_t *file);

int file_op_write(char *str, size_t str_size);

int file_op_unmount_disk();

#endif // FILE_OP_H_
