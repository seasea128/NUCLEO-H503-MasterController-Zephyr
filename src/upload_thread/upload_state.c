#include "upload_state.h"
#include "Protobuf-FYP/proto/data.pb.h"
#include "file_op.h"
#include "sim7600_driver.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(upload_state, CONFIG_LOG_DEFAULT_LEVEL);

void upload_state_init(upload_state *state, struct k_msgq *upload_data_msgq) {
    fs_file_t_init(&state->file);
    state->state = UPLOAD_STATE_DISCONNECTED;
    state->upload_data_msgq = upload_data_msgq;
}

// TODO: Make this non-blocking
void upload_state_execute(upload_state *state) {
    switch (state->state) {
    case UPLOAD_STATE_DISCONNECTED:
        // TODO: Reconnect to server
        LOG_DBG("State: disconnect");
        LOG_DBG("Switching state");
        state->state = UPLOAD_STATE_CONNECTED;
        break;
    case UPLOAD_STATE_CONNECTED: {
        LOG_DBG("State: conneceted");
        char msg[controllerMessage_Packet_size] = {0};
        LOG_INF("Remaining data: %d",
                k_msgq_num_used_get(state->upload_data_msgq));
        int result = k_msgq_get(state->upload_data_msgq, msg, K_FOREVER);
        if (result != 0) {
            LOG_WRN("Failed to retrieve message from queue: %d", result);
            return;
        }
        if (strlen(msg) == 0) {
            return;
        }

        LOG_INF("Message received: %s", msg);
        LOG_INF("Message size: %d", strlen(msg));
        result = sim7600_set_topic_publish_mqtt("/data", sizeof("/data"), msg,
                                                strlen(msg));
        if (result != SIM7600_OK) {
            LOG_ERR("Cannot publish message: %d", result);
            return;
        }
        break;
    case UPLOAD_STATE_READING_FILE:
    case UPLOAD_STATE_WAITING_DATA:
        break;
    }
    }
}
