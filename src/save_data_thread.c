#include "Protobuf-FYP/proto/data.pb.h"
#include "message.h"
#include "pb_encode.h"
#include "zephyr/fs/fs.h"
#include "zephyr/logging/log.h"
#include <stdbool.h>
#include <string.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(save_data_thread, CONFIG_LOG_DEFAULT_LEVEL);

#define DISK_DRIVE_NAME "SD"

#define DISK_MOUNT_PT "/" DISK_DRIVE_NAME ":"

extern struct k_fifo distance_save_fifo;
extern struct fs_file_t open_file;

void save_data_thread() {
    char disk_mount_pt[] = DISK_MOUNT_PT;

    static char fileName[128];
    {

        fs_file_t_init(&open_file);

        strncpy(fileName, disk_mount_pt, sizeof(fileName));

        int mountLength = strlen(disk_mount_pt);
        fileName[mountLength++] = '/';
        fileName[mountLength] = 0;

        strcat(&fileName[mountLength], "test.txt");

        LOG_INF("File path: %s", fileName);

        int ret = fs_open(&open_file, fileName,
                          FS_O_WRITE | FS_O_CREATE | FS_O_APPEND);
        if (ret != 0) {
            LOG_ERR("Cannot open file: %d", ret);
            return;
        }
    }

    message distance;
    int ret;
    uint8_t buffer[64];
    char str_write[64];
    pb_ostream_t stream;
    controllerMessage_DataReceived msg;

    stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    while (true) {
        // distance = *(uint16_t *)k_fifo_get(&distance_save_fifo, K_FOREVER);
        distance = *(message *)k_fifo_get(&distance_save_fifo, K_FOREVER);

        bool result =
            pb_encode(&stream, &controllerMessage_DataReceived_msg, &msg);
        if (!result) {
            LOG_ERR("Cannot create protobuf message: %d", ret);
            continue;
        }

        int size = sprintf(str_write, "%u,\n", distance.tl);

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
};
