#include "upload_state.h"
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
        sim7600_set_topic_mqtt("/data", sizeof("/data"));
        LOG_DBG("Switching state");
        state->state = UPLOAD_STATE_CONNECTED;
        break;
    case UPLOAD_STATE_CONNECTED: {
        LOG_DBG("State: conneceted");
        uint8_t msg[512];
        k_msgq_get(state->upload_data_msgq, msg, K_FOREVER);

        LOG_INF("Message received: %s", msg);

        sim7600_publish_mqtt(msg, strlen(msg));
        break;
    case UPLOAD_STATE_READING_FILE:
    case UPLOAD_STATE_WAITING_DATA:
        break;
    }
    }
}
