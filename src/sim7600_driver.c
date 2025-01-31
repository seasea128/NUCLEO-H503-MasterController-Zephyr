#include "sim7600_driver.h"
#include "sim7600_at_cmd.h"
#include "zephyr/logging/log.h"
#include "zephyr/toolchain.h"
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>

#define RX_TIMEOUT 500

LOG_MODULE_DECLARE(main, CONFIG_LOG_DEFAULT_LEVEL);

K_FIFO_DEFINE(sim7600_fifo);

const struct device *const dev = DEVICE_DT_GET(DT_ALIAS(modem));

// TODO: Mutex or semaphore?
struct k_mutex sim7600_mutex;
static char rx_buf[512];
static size_t rx_buf_pos = 0;
static size_t count = 0;

// TODO: Fix this callback, only reading 2-3 bytes and stopping
static void sim7600_callback(const struct device *dev, void *user_data) {
    ARG_UNUSED(user_data);

    uint8_t c;

    if (!uart_irq_update(dev)) {
        LOG_INF("UART IRQ not updating");
        return;
    }

    if (!uart_irq_rx_ready(dev)) {
        LOG_INF("UART RX not ready");
        return;
    }

    /* read until FIFO empty */
    while (uart_fifo_read(dev, &c, 1) == 1) {
        if ((c == '\n' || c == '\r') && rx_buf_pos > 0) {
            /* terminate string */
            rx_buf[rx_buf_pos] = '\0';

            /* if queue is full, message is silently dropped */
            k_fifo_put(&sim7600_fifo, &rx_buf);

            /* reset the buffer (it was copied to the msgq) */
            rx_buf_pos = 0;
        } else if (rx_buf_pos < (sizeof(rx_buf) - 1)) {
            rx_buf[rx_buf_pos++] = c;
        }
        /* else: characters beyond buffer size are dropped */
    }

    // while (uart_fifo_read(conf, &c, 1) == 1) {
    //     // The message ends with OK\r\n
    //     LOG_INF("Next char: 0x%x", c);
    //     // Ignore first set of \r\n
    //     if (c == '\n' && rx_buf_pos > 0) {
    //         // Check if the string inside is OK or not
    //         if (rx_buf[rx_buf_pos - 1] == 'K' &&
    //             rx_buf[rx_buf_pos - 2] == 'O') {
    //             /* terminate string */
    //             rx_buf[rx_buf_pos] = '\0';

    //            /* if queue is full, message is silently dropped */
    //            LOG_INF("Message received: %.*s", strlen(rx_buf), rx_buf);
    //            k_fifo_put(&sim7600_fifo, &rx_buf);
    //            k_mutex_unlock(&sim7600_mutex);

    //            /* reset the buffer (it was copied to the msgq) */
    //            rx_buf_pos = 0;
    //            count = 0;
    //        } else {
    //            LOG_INF("RX not finished but encounter \\r and \\n");
    //            rx_buf[rx_buf_pos++] = c;
    //        }
    //    } else if (rx_buf_pos < (sizeof(rx_buf) - 1)) {
    //        rx_buf[rx_buf_pos++] = c;
    //    }
    //    /* else: characters beyond buffer size are dropped */
    //}
}

SIM7600_RESULT
sim7600_init(char *mqtt_address, size_t addr_size) {
    LOG_INF("Initializing SIM7600");
    char at_output[256];

    _Static_assert(IS_ENABLED(CONFIG_UART_INTERRUPT_DRIVEN));
    if (!device_is_ready(dev)) {
        LOG_ERR("Cannot initialize SIM7600");
        return SIM7600_INIT_ERROR;
    }

    k_mutex_init(&sim7600_mutex);
    k_fifo_init(&sim7600_fifo);

    int ret = uart_irq_callback_set(dev, &sim7600_callback);
    if (ret != 0) {
        LOG_INF("UART Callback Error: %d", ret);
        return SIM7600_INIT_ERROR;
    }
    uart_irq_rx_enable(dev);
    // sim7600_send_at(ATE, sizeof(ATE), at_output, sizeof(at_output));

    // TODO: Set up MQTT connection and set the modem to correct mode
    sim7600_send_at("AT+CPSI?\r", sizeof("AT+CPSI?\r"), at_output,
                    sizeof(at_output));

    LOG_INF("Initializing SIM7600 done");
    return SIM7600_OK;
}

SIM7600_RESULT sim7600_send_at(char *cmd, size_t size_cmd, char *output,
                               size_t size_out) {
    LOG_INF("Sent AT command: %.*s", strlen(cmd), cmd);
    for (size_t i = 0; i < size_cmd; ++i) {
        uart_poll_out(dev, cmd[i]);
    }

    unsigned char *result =
        (unsigned char *)k_fifo_get(&sim7600_fifo, K_MSEC(RX_TIMEOUT));

    if (result == NULL) {
        LOG_ERR("SIM7600 RX timeout");
        return SIM7600_RX_TIMEOUT;
    }

    strncpy(output, result, size_out);

    LOG_INF("Result: %.*s", strlen(output), output);
    return SIM7600_OK;
}

SIM7600_RESULT sim7600_send_mqtt(uint8_t *buffer, size_t size) {
    // TODO: Add structure for using AT commands to send request
}
