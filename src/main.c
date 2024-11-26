#include <ff.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sd/sd_spec.h>
#include <zephyr/storage/disk_access.h>

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

CAN_MSGQ_DEFINE(distance_msgq, 10);

// TODO: msgq or fifo?
K_MSGQ_DEFINE(distance_save_msgq, sizeof(uint16_t), 100, 1);

const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

#define DISK_DRIVE_NAME "SD"

#define DISK_MOUNT_PT "/" DISK_DRIVE_NAME ":"

#define FS_RET_OK FR_OK
#define MAX_PATH 128

static FATFS fat_fs;
static struct fs_mount_t mp = {.type = FS_FATFS, .fs_data = &fat_fs};

static const char *disk_mount_pt = DISK_MOUNT_PT;

void save_to_fs_thread() {};

int main(void) {
    LOG_INF("Initializing");

    mp.mnt_point = disk_mount_pt;

    LOG_INF("Mounting disk");
    int res = fs_mount(&mp);

    if (res == FS_RET_OK) {
        LOG_INF("Disk mounted.\n");
        /* Try to unmount and remount the disk */
        res = fs_unmount(&mp);
        if (res != FS_RET_OK) {
            LOG_ERR("Error unmounting disk\n");
            return res;
        }
        res = fs_mount(&mp);
        if (res != FS_RET_OK) {
            LOG_ERR("Error remounting disk\n");
            return res;
        }
    } else {
        printk("Error mounting disk.\n");
        return res;
    }

    // Initializing FDCAN
    if (!device_is_ready(can_dev)) {
        LOG_ERR("FDCAN1 is not ready");
        return 12;
    }

    int ret = can_start(can_dev);
    if (ret != 0) {
        LOG_ERR("Cannot start FDCAN1: %d", ret);
    }

    const struct can_filter my_filter = {
        .flags = 0U, .id = 0x000, .mask = 0x03F8};

    int filter_id = can_add_rx_filter_msgq(can_dev, &distance_msgq, &my_filter);

    if (filter_id < 0) {
        LOG_ERR("Unable to add rx filter [%d]", filter_id);
    }

    static struct can_frame current_frame;
    static struct fs_file_t open_file;
    static char str_write[64];

    fs_file_t_init(&open_file);

    LOG_INF("Finished initializing");

    // TODO: Use onboard button to exit loop so that the filesystem can be
    // unmounted for data safety.
    while (true) {
        k_msgq_get(&distance_msgq, &current_frame, K_FOREVER);

        if (current_frame.dlc != 2) {
            LOG_ERR("Data length is not 2, continuing [%d]", current_frame.dlc);
            continue;
        }

        LOG_INF("Received distance: %u", *(uint16_t *)(&current_frame.data));

        k_msgq_put(&distance_save_msgq, (uint16_t *)(&current_frame.data),
                   K_FOREVER);

        // TODO: For testing purpose, final impl would probably with another
        // thread pulling data out from a queue, then save to file + transmit to
        // 4G module over UART.
        ret = fs_open(&open_file, "test.txt", FS_O_WRITE);
        if (ret != 0) {
            LOG_ERR("Cannot open file: %d", ret);
            continue;
        }

        ret = fs_seek(&open_file, 0, FS_SEEK_END);
        if (ret != 0) {
            LOG_ERR("Cannot seek file: %d", ret);
            continue;
        }

        int size =
            sprintf(str_write, "%u,\n", *(uint16_t *)(&current_frame.data));

        ret = fs_write(&open_file, str_write, size * sizeof(char));
        if (ret != 0) {
            LOG_ERR("Cannot write file: %d", ret);
            continue;
        }

        ret = fs_sync(&open_file);
        if (ret != 0) {
            LOG_ERR("Cannot sync file: %d", ret);
            continue;
        }
    }

    ret = fs_close(&open_file);
    if (ret != 0) {
        LOG_ERR("Cannot close file: %d", ret);
        return ret;
    }
    ret = fs_unmount(&mp);
    if (ret != 0) {
        LOG_ERR("Cannot unmount file: %d", ret);
        return ret;
    }
}
