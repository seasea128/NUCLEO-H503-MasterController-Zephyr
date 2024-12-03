#include "zephyr/types.h"
#include <ff.h>

int file_op_open_file(char *file_name);

int file_op_mount_disk();

int file_op_close_file();

int file_op_write(char *str, size_t str_size);

int file_op_unmount_disk();
