#include "zephyr/drivers/can.h"
#include "zephyr/device.h"
#include "zephyr/logging/log.h"

LOG_MODULE_DECLARE(main, CONFIG_LOG_DEFAULT_LEVEL);

extern struct k_msgq distance_msgq;

void can_callback(const struct device *dev, struct can_frame *frame,
                  void *user_data) {
    int ret = k_msgq_put(&distance_msgq, frame, K_NO_WAIT);
    if (ret != 0) {
        LOG_ERR("CAN message queue overrun: %d", ret);
    }
}

int can_init(struct k_msgq *message_queue, const struct device *can_def) {
    if (!device_is_ready(can_def)) {
        LOG_ERR("FDCAN1 is not ready");
        return 12;
    }

    int ret = can_start(can_def);
    if (ret != 0) {
        LOG_ERR("Cannot start FDCAN1: %d", ret);
        return ret;
    }

    const struct can_filter my_filter = {
        .flags = 0U, .id = 0x000, .mask = 0x03F8};

    // int filter_id = can_add_rx_filter_msgq(can_def, message_queue,
    // &my_filter);

    int filter_id = can_add_rx_filter(can_def, &can_callback, NULL, &my_filter);

    if (filter_id < 0) {
        LOG_ERR("Unable to add rx filter [%d]", filter_id);
    }

    LOG_INF("Finished initializing CAN");
    return ret;
}
