#include "upload_state.h"
#include "file_op.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(upload_state, CONFIG_LOG_DEFAULT_LEVEL);

void upload_state_init(upload_state *state) {
    fs_file_t_init(&state->file);
    state->state = UPLOAD_STATE_CONNECTED;
}

void upload_state_execute(upload_state *state) {
    switch (state->state) {
    case UPLOAD_STATE_DISCONNECTED:
        // TODO: Reconnect to server
        break;
    case UPLOAD_STATE_CONNECTED: {
        int file_count;
        int ret = file_op_get_count_in_dir("", &file_count);
        if (ret != 0) {
            LOG_ERR("Cannot get file count: %d", ret);
            return;
        }

        state->current_session_id = file_count;
        LOG_INF("Current session id: %d", state->current_session_id);

        char file_name[128] = {0};
        snprintf(file_name, sizeof(file_name), "%s-%d", CONTROLLER_NAME,
                 state->current_session_id);

        ret = file_op_open_file(&state->file, file_name, FS_O_RDWR);

        if (ret != 0) {
            LOG_ERR("Cannot open file: %d", ret);
            return;
        }

        // state->file_posix = fopen(file_name, "r+");

        state->state = UPLOAD_STATE_READING_FILE;
        break;
    }
    case UPLOAD_STATE_READING_FILE: {
        // TODO: Read data and send to server
        char command[256] = {0};
        char c = {0};
        size_t index = 0;
        size_t status = 0;

        do {
            status = fs_read(&state->file, &c, 1);
            if (c != '\n') {
                command[index++] = c;
            }
        } while (status == 1);

        command[index] = '\0';

        if (status == 0) {
            LOG_INF("EOF");
            state->state = UPLOAD_STATE_WAITING_DATA;
            return;
        } else if (status < 0) {
            LOG_ERR("Error reading file: %d", status);
            return;
        }

        // LOG_INF("Current line of data: %s", command);

        // TODO: If result is success then modify the flag and write

        break;
    }
    case UPLOAD_STATE_WAITING_DATA: {
        int file_count;
        int ret = file_op_get_count_in_dir("", &file_count);
        if (ret != 0) {
            LOG_ERR("Cannot get file count: %d", ret);
            return;
        }

        LOG_INF("New file count: %d, Current session id: %d", file_count,
                state->current_session_id);

        if (file_count == state->current_session_id) {
            // TODO: Wait for more entry in file
            state->state = UPLOAD_STATE_READING_FILE;
            k_sleep(K_MSEC(10));
            return;
        } else {
            state->current_session_id = file_count;

            char file_name[128] = {0};
            snprintf(file_name, sizeof(file_name), "%s-%d", CONTROLLER_NAME,
                     state->current_session_id);

            ret = file_op_open_file(&state->file, file_name, FS_O_RDWR);

            if (ret != 0) {
                LOG_ERR("Cannot open file: %d", ret);
                return;
            }

            // state->file_posix = fopen(file_name, "r+");

            state->state = UPLOAD_STATE_READING_FILE;
        }

        break;
    }
    }
}
