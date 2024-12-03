#include "zephyr/fs/fs.h"
#include "zephyr/fs/fs_interface.h"
#include "zephyr/logging/log.h"
#include <file_op.h>
#include <string.h>

LOG_MODULE_DECLARE(main, CONFIG_LOG_DEFAULT_LEVEL);

// SD setup
#define DISK_DRIVE_NAME "SD"

#define DISK_MOUNT_PT "/" DISK_DRIVE_NAME ":"

#define FS_RET_OK FR_OK
#define MAX_PATH 128

static FATFS fat_fs;
static struct fs_mount_t mp = {.type = FS_FATFS, .fs_data = &fat_fs};

static const char *disk_mount_pt = DISK_MOUNT_PT;

int file_op_open_file(char *file_name) {
    static char file_path[128];
    static struct fs_file_t open_file;
    fs_file_t_init(&open_file);

    strncpy(file_path, disk_mount_pt, sizeof(file_path));

    int mountLength = strlen(disk_mount_pt);
    file_path[mountLength++] = '/';
    file_path[mountLength] = 0;

    strcat(&file_path[mountLength], file_name);

    LOG_INF("File path: %s", file_path);

    int ret =
        fs_open(&open_file, file_path, FS_O_WRITE | FS_O_CREATE | FS_O_APPEND);
    if (ret != 0) {
        LOG_ERR("Cannot open file: %d", ret);
        return ret;
    }
    return ret;
}

int file_op_close_file() {
    static char file_path[128];
    static struct fs_file_t open_file;

    int ret = fs_sync(&open_file);
    if (ret != 0) {
        LOG_ERR("Cannot sync file: %d", ret);
        return ret;
    }

    LOG_INF("Closing file: %s", file_path);
    ret = fs_close(&open_file);
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
    mp.mnt_point = disk_mount_pt;

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
    return ret;
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
