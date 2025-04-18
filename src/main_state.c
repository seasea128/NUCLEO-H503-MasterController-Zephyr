#include "main_state.h"
#include "file_op.h"
#include "save_data_thread.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

// LOG_MODULE_REGISTER(main_state, CONFIG_LOG_DEFAULT_LEVEL);
LOG_MODULE_REGISTER(main_state, LOG_LEVEL_WRN);

inline static void record_data(main_state *state) {
    memset(&state->can_message, 0, sizeof(struct can_frame));
    int ret = k_msgq_get(state->can_msgq, &state->can_message, K_NO_WAIT);
    if (ret == -EAGAIN || ret == -ENOMSG) {
        return;
    } else if (ret < 0) {
        LOG_WRN("Cannot get message from queue: %d", ret);
        return;
    }

    LOG_INF("Message size: %d", state->can_message.dlc);

    if (state->can_message.dlc != 2) {
        LOG_ERR("Error: Data length is not 2, continuing [Length: %d]",
                state->can_message.dlc);
        return;
    }

    LOG_INF("Received distance: %u, Sensor ID: 0x%02x",
            *(uint16_t *)(&state->can_message.data), state->can_message.id);

    switch (state->can_message.id) {
    case 0x01:
        state->measurement.distance_lt =
            *(uint16_t *)(&state->can_message.data);
        break;
    case 0x02:
        state->measurement.distance_rt =
            *(uint16_t *)(&state->can_message.data);
        break;
    case 0x03:
        state->measurement.distance_lb =
            *(uint16_t *)(&state->can_message.data);
        break;
    case 0x04:
        state->measurement.distance_rb =
            *(uint16_t *)(&state->can_message.data);
        break;
    }

    state->dataPoints.measurement[state->dataPoints.measurement_count++] =
        state->measurement;

    if (state->dataPoints.measurement_count > 10) {
        // TODO: Get timestamp and location data
        // TODO: Write data
        state->dataPoints.measurement_count = 10;
        LOG_WRN("Saving/Sending new data");
        int ret = fs_seek(&state->file, 0, FS_SEEK_END);
        if (ret != 0) {
            LOG_ERR("Cannot seek file to end for writing: %d", ret);
            return;
        }
        write_data_points(&state->dataPoints, &state->file, state->upload_msgq);
    }
}

inline static void new_session(main_state *state) {
    if (state->file.mp != NULL) {
        // TODO: Write closing session packet and close file
        controllerMessage_Session session = controllerMessage_Session_init_zero;
        write_session(&session, &state->file, state->upload_msgq);
        int ret = file_op_close_file(&state->file);
        if (ret != 0) {
            LOG_ERR("Cannot close file: %d", ret);
            return;
        }
    }
    state->session_id++;
    state->dataPoints.session_id = state->session_id;

    char file_name[128] = {0};

    snprintf(file_name, sizeof(file_name), "%s-%d", CONTROLLER_NAME,
             state->session_id);

    int ret = file_op_open_file(&state->file, file_name, 0);
    if (ret != 0) {
        LOG_ERR("Cannot open file: %d", ret);
        return;
    }

    strncpy(state->dataPoints.controller_id, CONTROLLER_NAME,
            sizeof(state->dataPoints.controller_id));

    // TODO: Write start session packet
    controllerMessage_Session session = controllerMessage_Session_init_zero;
    strncpy(session.controller_id, CONTROLLER_NAME,
            sizeof(session.controller_id));
    session.isActive = true;
    session.session_id = state->session_id;

    ret = fs_seek(&state->file, 0, FS_SEEK_END);
    if (ret != 0) {
        LOG_ERR("Cannot seek file to end for writing: %d", ret);
        return;
    }
    write_session(&session, &state->file, state->upload_msgq);
    state->state = MAIN_STATE_RECORD_DATA;
}

void main_state_init(main_state *state, struct k_msgq *can_msgq,
                     struct k_msgq *upload_msgq) {
    fs_file_t_init(&state->file);
    state->can_msgq = can_msgq;
    state->upload_msgq = upload_msgq;
    state->state = MAIN_STATE_DISK_UNMOUNTED;
    memset(&state->dataPoints, 0, sizeof(controllerMessage_DataPoints));
    upload_state_init(&state->upload, upload_msgq);
}

void main_state_execute(main_state *state) {
    switch (state->state) {
    case MAIN_STATE_DISK_MOUNTED: {
        // TODO: wait until new session is triggered
        LOG_WRN("Disk mounted, starting new session");
        state->state = MAIN_STATE_NEW_SESSION;
        break;
    }
    case MAIN_STATE_DISK_UNMOUNTED: {
        LOG_WRN("Disk unmounted, mounting disk");
        int ret = file_op_mount_disk();
        if (ret < 0) {
            LOG_ERR("Cannot mount disk: %d", ret);
            return;
        }

        int file_count;
        ret = file_op_get_count_in_dir("", &file_count);
        if (ret < 0) {
            LOG_ERR("Cannot get file count: %d", ret);
            return;
        }

        state->session_id = file_count;

        state->state = MAIN_STATE_DISK_MOUNTED;
        break;
    }
    case MAIN_STATE_NEW_SESSION: {
        LOG_WRN("Starting new session");
        new_session(state);
        upload_state_execute(&state->upload, &state->file);
        break;
    }
    case MAIN_STATE_RECORD_DATA: {
        record_data(state);
        upload_state_execute(&state->upload, &state->file);
        break;
    }
    }
}
