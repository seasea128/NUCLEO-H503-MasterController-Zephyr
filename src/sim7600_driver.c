#include "sim7600_driver.h"
#include "sim7600_at_cmd.h"
#include "zephyr/kernel/thread_stack.h"
#include "zephyr/logging/log.h"
#include "zephyr/toolchain.h"
#include <stdio.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>

// TODO: Figure out what is the value of timeout that should be used
#define RX_TIMEOUT_MS 120000
#define STARTUP_SEQ_TIMEOUT_MS 30000

// TODO: Mutex or semaphore?
// struct k_mutex sim7600_mutex;
static char rx_buf[512];
static size_t rx_buf_pos = 0;
static bool MQTT_CONN = false;

#define SIM7600_WORKER_STACK 2048
#define SIM7600_PRIORITY 5
#define SIM7600_MSGQ_SIZE sizeof(sim7600_msgq_item)
#define SIM7600_MSGQ_MAX 10

LOG_MODULE_REGISTER(sim7600, LOG_LEVEL_DBG);

K_FIFO_DEFINE(sim7600_fifo);

K_THREAD_STACK_DEFINE(sim7600_stack, SIM7600_WORKER_STACK);

void sim7600_worker_handler();

K_THREAD_DEFINE(sim7600_worker_tid, SIM7600_WORKER_STACK,
                sim7600_worker_handler, NULL, NULL, NULL,
                K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);
static sim7600_work_info sim7600_rx_text;

K_MSGQ_DEFINE(sim7600_msgq, SIM7600_MSGQ_SIZE, SIM7600_MSGQ_MAX, 1);

const struct device *const dev = DEVICE_DT_GET(DT_ALIAS(modem));

// TODO: Just find \r\n and have a thread doing actual parsing?
static void sim7600_irq_handler(const struct device *dev, void *user_data) {
    ARG_UNUSED(user_data);

    uint8_t c;

    if (!uart_irq_update(dev)) {
        LOG_INF("UART IRQ not updating");
        return;
    }

    int result = uart_irq_rx_ready(dev);

    if (!result) {
        LOG_INF("UART RX not ready: %d", result);
        return;
    }

    // while (uart_fifo_read(dev, &c, 1) == 1) {
    //     // The message ends with OK\r\n
    //     if (c == '\r' && rx_buf_pos > 0) {
    //         if (rx_buf[rx_buf_pos - 1] == 'K' &&
    //             rx_buf[rx_buf_pos - 2] == 'O') {
    //             // Check if the string inside ends with OK or not
    //             /* terminate string */
    //             rx_buf[rx_buf_pos] = '\0';

    //            k_fifo_put(&sim7600_fifo, &rx_buf);

    //            /* reset the buffer (it was copied to the msgq) */
    //            rx_buf_pos = 0;
    //        } else if (rx_buf[rx_buf_pos - 1] == 'R' &&
    //                   rx_buf[rx_buf_pos - 2] == 'O') {
    //            // Checks for ERROR at the end of string
    //            /* terminate string */
    //            rx_buf[rx_buf_pos] = '\0';

    //            k_fifo_put(&sim7600_fifo, &rx_buf);

    //            /* reset the buffer (it was copied to the msgq) */
    //            rx_buf_pos = 0;
    //        } else if (rx_buf[rx_buf_pos - 1] == 'E' &&
    //                   rx_buf[rx_buf_pos - 2] == 'N') {
    //            // Startup ready check
    //            /* terminate string */
    //            rx_buf[rx_buf_pos] = '\0';

    //            k_fifo_put(&sim7600_fifo, &rx_buf);

    //            /* reset the buffer (it was copied to the msgq) */
    //            rx_buf_pos = 0;
    //        } else {
    //            LOG_INF("RX not finished but encounter \\r and \\n");
    //            if (c == '\r' || c == '\n')
    //                continue;
    //            rx_buf[rx_buf_pos++] = c;
    //        }
    //    } else if ((c == '0' && rx_buf[rx_buf_pos - 1] == ',') &&
    //               rx_buf_pos > 0 && MQTT_CONN) {
    //        // Weird case, OK comes first before response from modem.
    //        // Only for MQTTCONNECT and pub/sub command
    //        LOG_INF("MQTT connect detected");
    //        rx_buf[rx_buf_pos++] = c;
    //        rx_buf[rx_buf_pos] = '\0';
    //        k_fifo_put(&sim7600_fifo, &rx_buf);

    //        // Reset buffer and add current char
    //        rx_buf_pos = 0;
    //    } else if (rx_buf_pos < (sizeof(rx_buf) - 1)) {
    //        if (c == '\r' || c == '\n')
    //            continue;
    //        rx_buf[rx_buf_pos++] = c;
    //    }
    //    /* else: characters beyond buffer size are dropped */
    //}

    while (uart_fifo_read(dev, &c, 1) == 1) {
        if ((c == '\n') && rx_buf_pos > 0) {
            /* terminate string */
            rx_buf[rx_buf_pos] = '\0';

            /* if queue is full, message is silently dropped */
            // TODO: Put work into workqueue

            sim7600_msgq_item item;
            strncpy(item.msg, rx_buf, sizeof(rx_buf));
            LOG_INF("IRQ: %.*s", strlen(rx_buf), rx_buf);
            int result = k_msgq_put(&sim7600_msgq, (void *)&item, K_NO_WAIT);
            LOG_INF("MSGQ_PUT: %d", result);
            /* reset the buffer (it was copied to the msgq) */
            rx_buf_pos = 0;
        } else if (rx_buf_pos < (sizeof(rx_buf) - 1)) {
            rx_buf[rx_buf_pos++] = c;
        }
        /* else: characters beyond buffer size are dropped */
    }
}

extern void sim7600_worker_handler() {
    sim7600_msgq_item item;
    char hex[512];
    while (1) {
        int result = k_msgq_get(&sim7600_msgq, (void *)&item, K_FOREVER);
        LOG_INF("Result: %d, Length: %d, Message received: %.*s", result,
                strlen(item.msg), strlen(item.msg), item.msg);
        int i;
        for (i = 0; i < strlen(item.msg); i++) {
            sprintf(hex + (2 * i), "%02X", item.msg[i]);
        }
        hex[(2 * i) + 1] = '\0';
        LOG_INF("Message received in hex: %.*s", strlen(hex), hex);
    }
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

    k_fifo_init(&sim7600_fifo);
    k_thread_start(sim7600_worker_tid);

    int ret = uart_irq_callback_set(dev, &sim7600_irq_handler);
    if (ret != 0) {
        LOG_INF("UART Callback Error: %d", ret);
        return SIM7600_INIT_ERROR;
    }
    uart_irq_rx_enable(dev);

    // TODO: Restart device on init

    // Get 2 string from fifo on startup, need to do this since the modem takes
    // quite a bit of time to startup
    {
        unsigned char *result = (unsigned char *)k_fifo_get(
            &sim7600_fifo, K_MSEC(STARTUP_SEQ_TIMEOUT_MS));

        if (result != NULL) {
            LOG_INF("Result: %.*s", strlen(result), result);
        }

        unsigned char *result2 =
            (unsigned char *)k_fifo_get(&sim7600_fifo, K_MSEC(500));

        if (result != NULL) {
            LOG_INF("Result: %.*s", strlen(result2), result2);
        }
    }

    // Disable echo mode
    sim7600_send_at(ATE, sizeof(ATE), at_output, sizeof(at_output));

    k_sleep(K_MSEC(4000));

    // TODO: Set up MQTT connection and set the modem to correct mode
    sim7600_send_at("AT+COPS?\r", sizeof("AT+COPS?\r"), at_output,
                    sizeof(at_output));
    sim7600_send_at("AT+CSQ\r", sizeof("AT+CSQ\r"), at_output,
                    sizeof(at_output));

    sim7600_send_at(AT_MQTTSTART, sizeof(AT_MQTTSTART), at_output,
                    sizeof(at_output));

    // AT_MQTTACCQ
    {
        char at_mqttaccq[250] = {0};
        char clientName[] = "testClient";

        int size = snprintf(at_mqttaccq, sizeof(at_mqttaccq), AT_MQTTACCQ, 0,
                            sizeof(clientName), clientName);

        sim7600_send_at(at_mqttaccq, strlen(at_mqttaccq), at_output,
                        sizeof(at_output));
    }

    // AT_MQTTCONNECT
    {
        char at_mqttconnect[250] = {0};
        int size = snprintf(at_mqttconnect, sizeof(at_mqttconnect),
                            AT_MQTTCONNECT, 0, addr_size, mqtt_address, 60, 1);

        LOG_INF("MQTT_CONN = true");
        MQTT_CONN = true;
        sim7600_send_at(at_mqttconnect, strlen(at_mqttconnect), at_output,
                        sizeof(at_output));

        unsigned char *mqttConnectResult =
            (unsigned char *)k_fifo_get(&sim7600_fifo, K_MSEC(RX_TIMEOUT_MS));

        if (mqttConnectResult != NULL) {
            LOG_INF("MQTT connect result: %.*s", strlen(mqttConnectResult),
                    mqttConnectResult);
        }

        LOG_INF("MQTT_CONN = false");
        MQTT_CONN = false;
    }

    LOG_INF("Initializing SIM7600 done");
    return SIM7600_OK;
}

SIM7600_RESULT sim7600_close() {
    char at_result[250] = {0};

    {
        char mqtt_disconnect[30] = {0};

        int size = snprintf(mqtt_disconnect, sizeof(mqtt_disconnect),
                            AT_MQTTDIS, 0, 120);

        sim7600_send_at(mqtt_disconnect, strlen(mqtt_disconnect), at_result,
                        sizeof(at_result));
        LOG_INF("Disconnect client result: %.*s", strlen(at_result), at_result);
    }

    {
        char mqtt_rel[30] = {0};

        int size = snprintf(mqtt_rel, sizeof(mqtt_rel), AT_MQTTREL, 0);

        sim7600_send_at(mqtt_rel, strlen(mqtt_rel), at_result,
                        sizeof(at_result));
        LOG_INF("Release client result: %.*s", strlen(at_result), at_result);
    }

    sim7600_send_at(AT_MQTTSTOP, sizeof(AT_MQTTSTOP), at_result,
                    sizeof(at_result));
    LOG_INF("Stop MQTT service result: %.*s", strlen(at_result), at_result);

    return SIM7600_OK;
}

// TODO: 1 message at a time, no need for another thread?, just send the thing
// and parse output here. The command sent is already known so it would just be
// a big switch.
SIM7600_RESULT sim7600_send_at(char *cmd, size_t size_cmd, char *output,
                               size_t size_out) {
    LOG_INF("Sent AT command: %.*s", strlen(cmd), cmd);
    for (size_t i = 0; i < size_cmd; ++i) {
        uart_poll_out(dev, cmd[i]);
    }

    unsigned char *result =
        (unsigned char *)k_fifo_get(&sim7600_fifo, K_MSEC(RX_TIMEOUT_MS));

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

static void parse_response() {}
