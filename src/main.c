#include "Protobuf-FYP/proto/data.pb.h"
#include "main_state.h"
#include "upload_thread/upload_thread.h"
#include <can.h>
#include <ff.h>
#include <file_op.h>
#include <message.h>
#include <save_data_thread.h>
#include <sim7600_driver.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sd/sd_spec.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

struct k_fifo save_data_fifo;

// TODO: Check if stack size need to be increased
K_THREAD_STACK_DEFINE(save_data_stack, // 2048);
                      controllerMessage_Packet_size + 3072);
struct k_thread save_data_thread_data;
static k_tid_t upload_data_thread_id;

// CAN setup
CAN_MSGQ_DEFINE(distance_msgq, 100);
const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

static struct gpio_dt_spec button_gpio =
    GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw0), gpios, {0});

K_MSGQ_DEFINE(upload_data_msgq, controllerMessage_Packet_size, 25, 4);

int main(void) {
    LOG_INF("Initializing");
    int ret;
    main_state state;

    k_fifo_init(&save_data_fifo);

    // Initializing GPIO button on board
    ret = gpio_pin_configure_dt(&button_gpio, GPIO_INPUT);
    if (ret != 0) {
        LOG_ERR("Error %d: failed to configure %s pin %d", ret,
                button_gpio.port->name, button_gpio.pin);
        return 0;
    }

    ret = sim7600_init(SERVER_ADDR, sizeof(SERVER_ADDR));
    if (ret != SIM7600_OK) {
        LOG_ERR("Error %d: Cannot initialize SIM7600", ret);
        return ret;
    }

    LOG_INF("Initialized SIM7600");

    // Initializing FDCAN
    ret = can_init(&distance_msgq, can_dev);
    if (ret != 0) {
        LOG_ERR("Error %d: failed to initialize CAN", ret);
        return 0;
    }
    LOG_INF("Initialized CAN");

    main_state_init(&state, &distance_msgq, &upload_data_msgq);
    LOG_INF("Initialized main_state");

    // TODO: Get time from modem and set as internal time

    LOG_INF("Button port: %s pin: %d", button_gpio.port->name, button_gpio.pin);

    // file_op_mount_disk();

    bool button_state = false;

    LOG_INF("Creating thread");
    upload_data_thread_id = k_thread_create(
        &save_data_thread_data, save_data_stack,
        K_THREAD_STACK_SIZEOF(save_data_stack), upload_thread, NULL, NULL, NULL,
        K_LOWEST_APPLICATION_THREAD_PRIO, 0, K_NO_WAIT);
    k_thread_name_set(upload_data_thread_id, "upload_data");
    LOG_INF("Finish creating thread");

    // Use onboard button to exit loop so that the filesystem can be
    // unmounted for data safety.
    while (!button_state) {
        // Get button state
        button_state = gpio_pin_get_dt(&button_gpio);

        main_state_execute(&state);
    }

    file_op_unmount_disk();
    LOG_INF("Shutting down sim7600");
    sim7600_close();
    LOG_INF("Done shutting down sim7600");
}
