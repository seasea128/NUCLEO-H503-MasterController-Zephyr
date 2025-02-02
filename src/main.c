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

struct k_fifo distance_save_fifo;

char server_addr[] = "tcp://94.130.24.30:1883";

// CAN setup
CAN_MSGQ_DEFINE(distance_msgq, 10);
const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

static struct gpio_dt_spec button_gpio =
    GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw0), gpios, {0});

int main(void) {
    LOG_INF("Initializing");
    int ret;

    k_fifo_init(&distance_save_fifo);

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

    struct can_frame current_frame;

    ret = sim7600_init(server_addr, sizeof(server_addr));
    if (ret != SIM7600_OK) {
        LOG_ERR("Error %d: Cannot initialize SIM7600", ret);
        return ret;
    }

    k_sleep(K_MSEC(5000));

    sim7600_close();

    return 0;

    LOG_INF("Button port: %s pin: %d", button_gpio.port->name, button_gpio.pin);
    char str_write[64];

    file_op_mount_disk();
    ret = file_op_open_file("test.txt");
    if (ret != FR_OK) {
        LOG_ERR("Error %d: Cannot open file", ret);
        return ret;
    }

    bool button_state = false;

    message msg = message_init();

    // Use onboard button to exit loop so that the filesystem can be
    // unmounted for data safety.
    while (!button_state) {
        // Get button state
        button_state = gpio_pin_get_dt(&button_gpio);

        // Get data from message queue
        k_msgq_get(&distance_msgq, &current_frame, K_FOREVER);

        if (current_frame.dlc != 2) {
            LOG_ERR("Error: Data length is not 2, continuing [Length: %d]",
                    current_frame.dlc);
            continue;
        }

        LOG_INF("Received distance: %u", *(uint16_t *)(&current_frame.data));

        switch (current_frame.id) {
        case 0x01:
            msg.tl = *(uint16_t *)(&current_frame.data);
            break;
        case 0x02:
            msg.tr = *(uint16_t *)(&current_frame.data);
            break;
        case 0x03:
            msg.bl = *(uint16_t *)(&current_frame.data);
            break;
        case 0x04:
            msg.br = *(uint16_t *)(&current_frame.data);
            break;
        }

        if (msg.timestamp == 0) {
            // TODO: Get timestamp, speed and location from GPS
        }

        if (msg.tl != 0 && msg.tr != 0 && msg.bl != 0 && msg.br != 0) {
            // TODO: Send message over to SD card queue and 4G module queue
            k_fifo_put(&distance_save_fifo, &msg);
            memset(&msg, 0, sizeof(message));
        }

        // k_fifo_put(&distance_save_fifo, (uint16_t
        // *)(&current_frame.data));

        // TODO: For testing purpose, final impl would probably with
        // another thread pulling data out from a queue, then save to
        // file + transmit to 4G module over UART.

        int size =
            sprintf(str_write, "%u,\n", *(uint16_t *)(&current_frame.data));

        int write_size = file_op_write(str_write, size * sizeof(char));
        if (write_size < 0) {
            LOG_ERR("Cannot write file: %d", ret);
            continue;
        }
    }

    file_op_close_file();
    file_op_unmount_disk();
}

// K_THREAD_DEFINE(save_data_thread_id, 1024, save_data_thread, NULL, NULL,
// NULL,
//                 7, 0, 0);
