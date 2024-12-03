#include <can.h>
#include <ff.h>
#include <file_op.h>
#include <save_data_thread.h>
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

// TODO: msgq or fifo? probably fifo
// K_FIFO_DEFINE(distance_save_fifo);
struct k_fifo distance_save_fifo;

// CAN setup
CAN_MSGQ_DEFINE(distance_msgq, 10);

static struct gpio_dt_spec button_gpio =
    GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw0), gpios, {0});

int main(void) {
    LOG_INF("Initializing");
    int ret;

    k_fifo_init(&distance_save_fifo);

    can_init();

    file_op_mount_disk();

    // Initializing GPIO button on board
    ret = gpio_pin_configure_dt(&button_gpio, GPIO_INPUT);
    if (ret != 0) {
        LOG_ERR("Error %d: failed to configure %s pin %d", ret,
                button_gpio.port->name, button_gpio.pin);
        return 0;
    }

    // Initializing FDCAN
    static struct can_frame current_frame;

    LOG_INF("Button port: %s pin: %d", button_gpio.port->name, button_gpio.pin);
    static char str_write[64];

    ret = file_op_open_file("test.txt");
    if (ret != FR_OK) {
        LOG_ERR("Cannot open file");
        return ret;
    }

    bool button_state = true;

    // TODO: Use onboard button to exit loop so that the filesystem can be
    // unmounted for data safety.
    while (button_state) {
        LOG_INF("Current button state: %d", button_state);
        button_state = !gpio_pin_get_dt(&button_gpio);
        LOG_INF("Current button state: %d", button_state);
        k_msgq_get(&distance_msgq, &current_frame, K_FOREVER);

        if (current_frame.dlc != 2) {
            LOG_ERR("Data length is not 2, continuing [%d]", current_frame.dlc);
            continue;
        }

        LOG_INF("Received distance: %u", *(uint16_t *)(&current_frame.data));

        // k_msgq_put(&distance_save_msgq, (uint16_t *)(&current_frame.data),
        // K_FOREVER);

        // k_fifo_put(&distance_save_fifo, (uint16_t *)(&current_frame.data));

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
