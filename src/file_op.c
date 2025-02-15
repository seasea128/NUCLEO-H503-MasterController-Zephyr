#include "zephyr/fs/fs.h"
#include "zephyr/fs/fs_interface.h"
#include "zephyr/logging/log.h"
#include <file_op.h>
#include <stdio.h>
#include <string.h>

LOG_MODULE_REGISTER(file_op, CONFIG_LOG_DEFAULT_LEVEL);

// SD setup
#define DISK_DRIVE_NAME "SD"

#define DISK_MOUNT_PT "/" DISK_DRIVE_NAME ":"

#define FS_RET_OK FR_OK
#define MAX_PATH 128

static FATFS fat_fs;

static const char *disk_mount_pt = DISK_MOUNT_PT;
static struct fs_mount_t mp = {
    .type = FS_FATFS, .fs_data = &fat_fs, .mnt_point = DISK_MOUNT_PT};

static int get_path(char *file_path, size_t path_size, char *file_name) {
    strncpy(file_path, disk_mount_pt, sizeof(path_size));

    int mountLength = strlen(disk_mount_pt);
    if (file_name != NULL) {
        file_path[mountLength++] = '/';
        file_path[mountLength] = 0;
        strcat(&file_path[mountLength], file_name);
    }

    LOG_INF("File path: %s", file_path);
    LOG_INF("Mount point: %s", mp.mnt_point);
}

int file_op_open_file(struct fs_file_t *file, char *file_name,
                      fs_mode_t flags) {
    char file_path[MAX_PATH];
    fs_file_t_init(file);
    get_path(file_path, sizeof(file_path), file_name);

    int ret = fs_open(file, file_path, FS_O_WRITE | FS_O_CREATE | flags);
    if (ret != 0) {
        LOG_ERR("Cannot open file: %d", ret);
        return ret;
    }
    return ret;
}

int file_op_get_count_in_dir(char *path, int *file_count) {
    struct fs_dir_t dir;
    char file_path[MAX_PATH] = {0};
    get_path(file_path, sizeof(file_path), path);
    fs_dir_t_init(&dir);
    int ret = fs_opendir(&dir, file_path);
    if (ret != 0) {
        LOG_ERR("Cannot open directory: %d", ret);
        return ret;
    }

    struct fs_dirent entry;
    int count = 0;
    while (true) {
        ret = fs_readdir(&dir, &entry);

        if (ret || entry.name[0] == 0) {
            break;
        }

        count++;
    }

    ret = fs_closedir(&dir);
    if (ret != 0) {
        LOG_ERR("Cannot close directory: %d", ret);
        return ret;
    }

    *file_count = count;
    return ret;
}

int file_op_close_file(struct fs_file_t *file) {
    static char file_path[128];

    int ret = fs_sync(file);
    if (ret != 0) {
        LOG_ERR("Cannot sync file: %d", ret);
        return ret;
    }

    LOG_INF("Closing file: %s", file_path);
    ret = fs_close(file);
    if (ret != 0) {
        LOG_ERR("Cannot close file: %d", ret);
        return ret;
    }
    return ret;
}

int file_op_write(char *str, size_t str_size) {
    static struct fs_file_t open_file;
    int write_size = fs_write(&open_file, str, str_size);
    if (write_size < 0) {
        LOG_ERR("Cannot write file: %d", write_size);
        return write_size;
    }
    return write_size;
}

int file_op_mount_disk() {
    LOG_INF("Mounting disk");
    int ret = fs_mount(&mp);

    if (ret == FS_RET_OK) {
        LOG_INF("Disk mounted.");
        /* Try to unmount and remount the disk */
        ret = fs_unmount(&mp);
        if (ret != FS_RET_OK) {
            LOG_ERR("Error unmounting disk");
            return ret;
        }
        ret = fs_mount(&mp);
        if (ret != FS_RET_OK) {
            LOG_ERR("Error remounting disk");
            return ret;
        }
    } else {
        LOG_ERR("Error mounting disk.");
        return ret;
    }
}

int file_op_unmount_disk() {
    LOG_INF("Unmounting fs");
    int ret = fs_unmount(&mp);
    if (ret != 0) {
        LOG_ERR("Cannot unmount file: %d", ret);
        return ret;
    }
    return ret;
}
