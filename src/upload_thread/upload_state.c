#include "upload_state.h"
#include "Protobuf-FYP/proto/data.pb.h"
#include "sim7600_driver.h"
#include "zephyr/fs/fs.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(upload_state, LOG_LEVEL_INF);

void upload_state_init(upload_state *state, struct k_msgq *upload_data_msgq) {
    state->state = UPLOAD_STATE_DISCONNECTED;
    state->upload_file_offset = 0;
    memset(state->data, 0, sizeof(state->data));
}

// TODO: Make this non-blocking
void upload_state_execute(upload_state *state, struct fs_file_t *file) {
    switch (state->state) {
    case UPLOAD_STATE_DISCONNECTED:
        // TODO: Reconnect to server
        LOG_DBG("State: disconnect");
        LOG_DBG("Switching state");
        state->state = UPLOAD_STATE_CONNECTED_READING_FILE;
        break;
    case UPLOAD_STATE_CONNECTED_SEND: {
        LOG_DBG("State: conneceted");
        size_t data_len = strlen(state->data);

        LOG_INF("Message received: %s", state->data);
        LOG_INF("Message size: %d", data_len);
        int result = sim7600_set_topic_publish_mqtt("/data", sizeof("/data"),
                                                    state->data, data_len);
        if (result != SIM7600_OK) {
            LOG_ERR("Cannot publish message: %d", result);
            return;
        }

        LOG_INF("Switching state");
        // TODO: Switch to receiving state
        state->state = UPLOAD_STATE_CONNECTED_RECEIVING;
        break;
    }
    case UPLOAD_STATE_CONNECTED_RECEIVING:
        // TODO: Check when response come back
        state->state = UPLOAD_STATE_CONNECTED_READING_FILE;
        break;
    case UPLOAD_STATE_CONNECTED_READING_FILE:
        LOG_DBG("State: Reading file");
        LOG_DBG("Seeking file");
        off_t prev_offset = fs_tell(file);
        if (prev_offset <= state->upload_file_offset) {
            LOG_DBG("Upload offset is higher than end of file");
            LOG_DBG("prev_offset: %ld, upload_file_offset: %d", prev_offset,
                    state->upload_file_offset);
            break;
        }

        int ret = fs_seek(file, state->upload_file_offset, FS_SEEK_SET);
        if (ret < 0) {
            LOG_WRN("Cannot seek file when reading: %d", ret);
            break;
        }

        state->data_index = 0;
        char c = 0;
        while (c != '\n') {
            ret = fs_read(file, &c, 1);
            if (ret < 0) {
                LOG_WRN("Cannot read byte: %d", ret);
                break;
            }
            LOG_DBG("count: %d, char: %c : %x", state->data_index, c, c);
            state->data[state->data_index++] = c;
            if (state->data_index >= controllerMessage_Packet_size) {
                LOG_WRN("Index exceed the array size: %d", state->data_index);
                break;
            }
        }

        state->data[state->data_index] = '\0';
        LOG_INF("Data read: %s", state->data);

        off_t new_offset = fs_tell(file);
        if (new_offset < 0) {
            LOG_WRN("Cannot get new offset: %ld", new_offset);
            break;
        }
        LOG_DBG("New offset: %d", new_offset);

        state->upload_file_offset = new_offset;
        state->state = UPLOAD_STATE_CONNECTED_SEND;
        break;
    case UPLOAD_STATE_CONNECTED_WAITING_DATA:
        break;
    }
}
