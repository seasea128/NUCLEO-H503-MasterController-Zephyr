#include "zephyr/device.h"
#include "zephyr/kernel.h"

int can_init(struct k_msgq *message_queue, const struct device *can_def);
