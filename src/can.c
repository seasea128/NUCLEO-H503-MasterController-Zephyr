#include "zephyr/drivers/can.h"
#include "zephyr/device.h"
#include "zephyr/logging/log.h"

LOG_MODULE_DECLARE(main, CONFIG_LOG_DEFAULT_LEVEL);

extern struct k_msgq distance_msgq;

const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

int can_init() {
    if (!device_is_ready(can_dev)) {
        LOG_ERR("FDCAN1 is not ready");
        return 12;
    }

    int ret = can_start(can_dev);
    if (ret != 0) {
        LOG_ERR("Cannot start FDCAN1: %d", ret);
        return ret;
    }

    const struct can_filter my_filter = {
        .flags = 0U, .id = 0x000, .mask = 0x03F8};

    int filter_id = can_add_rx_filter_msgq(can_dev, &distance_msgq, &my_filter);

    if (filter_id < 0) {
        LOG_ERR("Unable to add rx filter [%d]", filter_id);
    }

    LOG_INF("Finished initializing CAN");
    return ret;
}
