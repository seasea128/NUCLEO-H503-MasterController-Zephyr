#include "upload_state.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(upload_thread, CONFIG_LOG_DEFAULT_LEVEL);

extern struct k_msgq upload_data_msgq;

void upload_thread() {
    upload_state state;
    // upload_state_init(&state, &upload_data_msgq);
    while (true) {
        // upload_state_execute(&state);
    }
}
