#include "Protobuf-FYP/proto/data.pb.h"
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

char server_addr[] = "tcp://94.130.24.30:1883";

// TODO: Check if stack size need to be increased
K_THREAD_STACK_DEFINE(save_data_stack, // 2048);
                      controllerMessage_Packet_size + 4096);
struct k_thread save_data_thread_data;
static k_tid_t save_data_thread_id;

// CAN setup
CAN_MSGQ_DEFINE(distance_msgq, 100);
const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
static struct can_frame current_frame;

static struct gpio_dt_spec button_gpio =
    GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw0), gpios, {0});

int main(void) {
    LOG_INF("Initializing");
    int ret;
    int32_t session_id = 0;

    k_fifo_init(&save_data_fifo);

    // Initializing GPIO button on board
    ret = gpio_pin_configure_dt(&button_gpio, GPIO_INPUT);
    if (ret != 0) {
        LOG_ERR("Error %d: failed to configure %s pin %d", ret,
                button_gpio.port->name, button_gpio.pin);
        return 0;
    }

    // Initializing FDCAN
    ret = can_init(&distance_msgq, can_dev);
    if (ret != 0) {
        LOG_ERR("Error %d: failed to initialize CAN", ret);
        return 0;
    }

    // ret = sim7600_init(server_addr, sizeof(server_addr));
    // if (ret != SIM7600_OK) {
    //     LOG_ERR("Error %d: Cannot initialize SIM7600", ret);
    //     return ret;
    // }

    // TODO: Get time from modem and set as internal time

    LOG_INF("Button port: %s pin: %d", button_gpio.port->name, button_gpio.pin);

    file_op_mount_disk();

    // TODO: Send session id to server
    ret = file_op_get_count_in_dir(NULL, &session_id);
    if (ret != 0) {
        LOG_ERR("Error %d: failed to get file count in SD card", ret);
        return 0;
    }
    LOG_INF("Current file count: %d", session_id);
    session_id++;
    LOG_INF("Current session id: %d", session_id);

    bool button_state = false;

    controllerMessage_Measurement measurement =
        controllerMessage_Measurement_init_zero;

    LOG_INF("Creating thread");
    save_data_thread_id = k_thread_create(
        &save_data_thread_data, save_data_stack,
        K_THREAD_STACK_SIZEOF(save_data_stack), save_data_thread, NULL, NULL,
        NULL, K_LOWEST_APPLICATION_THREAD_PRIO, 0, K_NO_WAIT);
    LOG_INF("Finish creating thread");

    // Autostart new session when device start
    {
        message new_session = {.msg_type = message_new_session,
                               .data.session_id = session_id};
        k_fifo_put(&save_data_fifo, &new_session);
    }
    k_sleep(K_MSEC(300));

    // Use onboard button to exit loop so that the filesystem can be
    // unmounted for data safety.
    while (!button_state) {
        // Get button state
        button_state = gpio_pin_get_dt(&button_gpio);

        // Get data from message queue
        // k_msgq_get(&distance_msgq, &current_frame, K_FOREVER);

        // if (current_frame.dlc != 2) {
        //     LOG_ERR("Error: Data length is not 2, continuing [Length: %d]",
        //             current_frame.dlc);
        //     continue;
        // }

        //.LOG_INF("Received distance: %u", *(uint16_t *)(&current_frame.data));

        // switch (current_frame.id) {
        // case 0x01:
        //     measurement.distance_lt = *(uint16_t *)(&current_frame.data);
        //     break;
        // case 0x02:
        //     measurement.distance_rt = *(uint16_t *)(&current_frame.data);
        //     break;
        // case 0x03:
        //     measurement.distance_lb = *(uint16_t *)(&current_frame.data);
        //     break;
        // case 0x04:
        //     measurement.distance_rb = *(uint16_t *)(&current_frame.data);
        //     break;
        // }
        measurement.distance_lt = 100;

        // TODO: timestamp per measurement
        //  if (measurement.timestamp == 0) {
        //      measurement.timestamp = k_uptime_get();
        //  }

        if (measurement.distance_lt != 0
            // && measurement.timestamp != 0
            // && measurement.tr != 0
            // && measurement.bl != 0
            // && measurement.br != 0
        ) {
            // TODO: Send message over to SD card queue and 4G module queue
            message msg = {.msg_type = message_measurement,
                           .data.measurement = measurement};
            k_fifo_put(&save_data_fifo, &msg);
            memset(&measurement, 0, sizeof(controllerMessage_Measurement));
        }
        k_sleep(K_MSEC(10));

        // TODO: For testing purpose, final impl would probably with
        // another thread pulling data out from a queue, then save to
        // file + transmit to 4G module over UART.

        // char str_write[64];
        // int size =
        //     sprintf(str_write, "%u,\n", *(uint16_t *)(&current_frame.data));

        // LOG_INF("CAN message: %.*s", size, str_write);
    }

    file_op_unmount_disk();
    LOG_INF("Shutting down sim7600");
    sim7600_close();
    LOG_INF("Done shutting down sim7600");
}
