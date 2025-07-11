#include "upload_state.h"
#include "Protobuf-FYP/proto/data.pb.h"
#include "assert.h"
#include "sim7600_driver.h"
#include "zephyr/fs/fs.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(upload_state, LOG_LEVEL_INF);

void upload_state_init(upload_state *state, struct k_msgq *upload_data_msgq) {
    state->state = UPLOAD_STATE_DISCONNECTED;
    state->ok_detected = false;
    state->data_index = 0;
    state->current_session_id = 0;
    state->upload_file_offset = 0;
    memset(state->data, 0, sizeof(state->data));
    memset(state->modem_output, 0, sizeof(state->modem_output));
}

static void upload_state_receiving(upload_state *state) {
    assert(sizeof(state->modem_output) == 256);
    SIM7600_RESULT response = sim7600_check_resp(
        state->modem_output, sizeof(state->modem_output), state->ok_detected);
    switch (response) {
    case SIM7600_OK: {
        LOG_INF("SIM7600_OK received");
        if (state->ok_detected) {
            // TODO: switch state to reading file
            LOG_INF("switching to UPLOAD_STATE_CONNECTED_READING_FILE");
            state->state = UPLOAD_STATE_CONNECTED_READING_FILE;
            memset(state->modem_output, 0, sizeof(state->modem_output));
        }
        break;
    }
    case SIM7600_OK_DETECTED: {
        LOG_INF("SIM7600_OK_DETECTED received");
        state->ok_detected = true;
        break;
    }
    case SIM7600_NO_NEW_DATA:
        LOG_DBG("No new data");
        break;
    default:
        LOG_INF("other returncode received: %d", response);
        break;
    }
}

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

        LOG_INF("Message size: %d", data_len);
        int result = sim7600_set_topic_publish_mqtt("/data", sizeof("/data"),
                                                    state->data, data_len);
        if (result != SIM7600_OK) {
            LOG_ERR("Cannot publish message: %d", result);
            return;
        }

        LOG_INF("Switching state");
        state->state = UPLOAD_STATE_CONNECTED_RECEIVING;
        break;
    }
    case UPLOAD_STATE_CONNECTED_RECEIVING:
        upload_state_receiving(state);
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

        state->data[state->data_index - 1] = '\0';
        LOG_INF("Data read: %d", state->data_index);

        off_t new_offset = fs_tell(file);
        if (new_offset < 0) {
            LOG_WRN("Cannot get new offset: %ld", new_offset);
            break;
        }
        LOG_DBG("New offset: %ld", new_offset);

        state->upload_file_offset = new_offset;
        state->state = UPLOAD_STATE_CONNECTED_SEND;
        break;
    case UPLOAD_STATE_CONNECTED_WAITING_DATA:
        break;
    }
}
