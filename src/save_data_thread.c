#include "Protobuf-FYP/proto/data.pb.h"
#include "file_op.h"
#include "message.h"
#include "pb_encode.h"
#include "sim7600_driver.h"
#include "zephyr/fs/fs.h"
#include "zephyr/logging/log.h"
#include <stdbool.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/base64.h>

LOG_MODULE_REGISTER(save_data_thread, LOG_LEVEL_WRN);

#define DISK_DRIVE_NAME "SD"

#define DISK_MOUNT_PT "/" DISK_DRIVE_NAME ":"

extern struct k_fifo save_data_fifo;

static controllerMessage_DataPoints dataPoints =
    controllerMessage_DataPoints_init_zero;

static uint8_t buffer[controllerMessage_DataPoints_size] = {0};
static pb_ostream_t stream;

static void write_data_points(controllerMessage_DataPoints *dataPoints,
                              struct fs_file_t *file) {
    memset(&buffer, 0, sizeof(buffer));
    memset(&stream, 0, sizeof(pb_ostream_t));
    bool result =
        pb_encode(&stream, &controllerMessage_DataPoints_msg, dataPoints);
    if (!result) {
        LOG_ERR("Cannot create protobuf message: %s", PB_GET_ERROR(&stream));
        return;
    }
    size_t base64_len;
    base64_encode(NULL, 0, &base64_len, buffer, stream.bytes_written);
    LOG_INF("Base64 length: %d", base64_len);

    // Convert to Base64
    char base64_out[base64_len + 2];
    int ret = base64_encode(base64_out, sizeof(base64_out), &base64_len, buffer,
                            stream.bytes_written);
    if (ret) {
        LOG_ERR("Base64 encoding failed: %d", ret);
        return;
    }

    base64_out[++base64_len] = '\n';

    LOG_INF("Data point packet: %.*s", strlen(base64_out), base64_out);
    LOG_HEXDUMP_INF(base64_out, strlen(base64_out), "Packet in hex");

    // Write to SD card
    // ret = fs_seek(file, 0, FS_SEEK_END);
    // if (ret != 0) {
    //    LOG_ERR("Cannot seek to end of file: %d", ret);
    //    goto cleanup;
    //}

    ret = fs_write(file, base64_out, base64_len);
    if (ret < 0) {
        LOG_ERR("Cannot write to file: %d", ret);
        goto cleanup;
    }

    // ret = fs_sync(file);
    // if (ret < 0) {
    //     LOG_ERR("Cannot write to file: %d", ret);
    //     return;
    // }

cleanup:
    // Clear struct
    memset(&dataPoints->measurement, 0, sizeof(dataPoints->measurement));
    dataPoints->measurement_count = 0;
}

static void write_session(controllerMessage_Session *session,
                          struct fs_file_t *file) {
    memset(&buffer, 0, sizeof(buffer));
    memset(&stream, 0, sizeof(pb_ostream_t));
    bool result = pb_encode(&stream, &controllerMessage_Session_msg, session);
    if (!result) {
        LOG_ERR("Cannot create protobuf message: %s", PB_GET_ERROR(&stream));
        return;
    }
    size_t base64_len;
    base64_encode(NULL, 0, &base64_len, buffer, stream.bytes_written);
    LOG_INF("Base64 length: %d", base64_len);

    // Convert to Base64
    char base64_out[base64_len + 2];
    int ret = base64_encode(base64_out, sizeof(base64_out), &base64_len, buffer,
                            stream.bytes_written);
    if (ret) {
        LOG_ERR("Base64 encoding failed: %d", ret);
        return;
    }

    LOG_HEXDUMP_INF(base64_out, strlen(base64_out),
                    "Packet in hex before newline");

    base64_out[++base64_len] = '\n';

    LOG_INF("Session packet: %.*s", strlen(base64_out), base64_out);
    LOG_HEXDUMP_INF(base64_out, strlen(base64_out), "Packet in hex");

    // Write to SD card
    // ret = fs_seek(file, 0, FS_SEEK_END);
    // if (ret != 0) {
    //    LOG_ERR("Cannot seek to end of file: %d", ret);
    //    return;
    //}

    ret = fs_write(file, base64_out, base64_len);
    if (ret < 0) {
        LOG_ERR("Cannot write to file: %d", ret);
        return;
    }

    // ret = fs_sync(file);
    // if (ret < 0) {
    //     LOG_ERR("Cannot write to file: %d", ret);
    //     return;
    // }
}

void save_data_thread() {
    LOG_INF("Save Data Thread started");

    struct fs_file_t file;
    fs_file_t_init(&file);
    // message msg;
    controllerMessage_Session session = controllerMessage_Session_init_zero;
    strncpy(session.controller_id, CONTROLLER_NAME,
            sizeof(session.controller_id));

    stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    while (true) {
        message msg = *(message *)k_fifo_get(&save_data_fifo, K_FOREVER);

        switch (msg.msg_type) {
        case message_measurement: {
            // LOG_INF("Measurement data received");
            dataPoints.measurement[dataPoints.measurement_count++] =
                msg.data.measurement;
            if (dataPoints.measurement_count >= 10) {
                // LOG_INF("Enough message logged, saving to disk");
                if (strlen(dataPoints.controller_id) == 0) {
                    strncpy(dataPoints.controller_id, CONTROLLER_NAME,
                            sizeof(dataPoints.controller_id));
                }
                write_data_points(&dataPoints, &file);
            }
            break;
        }
        case message_new_session: {
            LOG_INF("New session message received");

            if (dataPoints.measurement_count != 0) {
                write_data_points(&dataPoints, &file);
            }

            char file_name[128] = {0};
            snprintf(file_name, sizeof(file_name), "%s-%d.txt", CONTROLLER_NAME,
                     msg.data.session_id);

            if (file.filep != NULL) {
                LOG_INF("File is not null");
                session.isActive = false;
                // TODO: Add timestamp
                session.timestamp.nanos = 0;
                session.timestamp.seconds = 0;
                write_session(&session, &file);
                file_op_close_file(&file);
            }

            session.session_id = msg.data.session_id;
            session.isActive = true;
            // TODO: Add timestamp
            session.timestamp.nanos = 0;
            session.timestamp.seconds = 0;

            file_op_open_file(&file, file_name, FS_O_APPEND);
            LOG_INF("Opened new file");
            write_session(&session, &file);
            break;
        }
        case message_undefined: {
            LOG_ERR("Undefined message received");
            break;
        }
        }
    }
}
