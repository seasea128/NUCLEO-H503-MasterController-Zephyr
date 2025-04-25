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

// Timeout from SIM7600 AT Reference Manual
#define RX_TIMEOUT_MS 120000

// Timeout from testing with actual product
#define STARTUP_SEQ_TIMEOUT_MS 30000

static char rx_buf[BUFFER_SIZE];
static size_t rx_buf_pos = 0;

#define SIM7600_WORKER_STACK 3072
#define SIM7600_PRIORITY 5
#define SIM7600_MSGQ_SIZE sizeof(sim7600_msgq_item)
#define SIM7600_MSGQ_MAX 10

LOG_MODULE_REGISTER(sim7600, LOG_LEVEL_WRN);

// Queues
K_FIFO_DEFINE(sim7600_fifo);
// K_MSGQ_DEFINE(sim7600_msgq, SIM7600_MSGQ_SIZE, SIM7600_MSGQ_MAX, 1);

// Threads
void sim7600_worker_handler();
// K_THREAD_DEFINE(sim7600_worker_tid, SIM7600_WORKER_STACK,
//                 sim7600_worker_handler, NULL, NULL, NULL,
//                 K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);

// Mutexes
static struct k_mutex sim7600_mutex;

// Modem from device tree
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

    while (uart_fifo_read(dev, &c, 1) == 1) {
        if ((c == '\n') && rx_buf_pos > 0) {
            /* terminate string */
            rx_buf[rx_buf_pos] = '\0';

            /* if queue is full, message is silently dropped */
            // TODO: Put work into workqueue

            sim7600_msgq_item item;
            strncpy(item.msg, rx_buf, sizeof(rx_buf));
            LOG_INF("IRQ: %.*s", strlen(rx_buf), rx_buf);
            //  int result = k_msgq_put(&sim7600_msgq, (void *)&item,
            //  K_NO_WAIT);
            k_fifo_put(&sim7600_fifo, item.msg);
            // LOG_INF("MSGQ_PUT: %d", result);
            /* reset the buffer (it was copied to the msgq) */
            rx_buf_pos = 0;
        } else if (c == '>') {
            rx_buf[rx_buf_pos++] = c;
            /* terminate string */
            rx_buf[rx_buf_pos] = '\0';

            /* if queue is full, message is silently dropped */
            // TODO: Put work into workqueue

            sim7600_msgq_item item;
            strncpy(item.msg, rx_buf, sizeof(rx_buf));
            // LOG_INF("IRQ: %.*s", strlen(rx_buf), rx_buf);
            // int result = k_msgq_put(&sim7600_msgq, (void *)&item, K_NO_WAIT);
            k_fifo_put(&sim7600_fifo, item.msg);
            // LOG_INF("MSGQ_PUT: %d", result);
            /* reset the buffer (it was copied to the msgq) */
            rx_buf_pos = 0;
        } else if (rx_buf_pos < (sizeof(rx_buf) - 1)) {
            if (c == '\r' || c == '\n')
                continue;
            rx_buf[rx_buf_pos++] = c;
        }
        /* else: characters beyond buffer size are dropped */
    }
}

// TODO: Remove this and only use 1 buffer, no need to response to server side
// command
// This might be needed if URC need to be handler
// Or use modem_chat infra?
// extern void sim7600_worker_handler() {
//    sim7600_msgq_item item;
//    // char hex[512];
//    while (1) {
//        int result = k_msgq_get(&sim7600_msgq, (void *)&item, K_FOREVER);
//        LOG_INF("Result: %d, Length: %d, Message received: %.*s", result,
//                strlen(item.msg), strlen(item.msg), item.msg);
//        if (item.msg[0] == '\r' && strlen(item.msg) == 1) {
//            // LOG_INF("Detected \\r, skipping");
//            continue;
//        }
//
//        for (int i = 0; i < strlen(item.msg); i++) {
//            if (item.msg[i] == '\r')
//                item.msg[i] = ' ';
//        }
//
//        k_fifo_put(&sim7600_fifo, item.msg);
//    }
//}

static SIM7600_RESULT startup_parse() {
    LOG_INF("Waiting for startup");
    int done_count = 0;
    char resp_buf[128] = {0};
    resp_buf[0] = '\0';
    while (true) {
        // LOG_INF("Reading data");
        unsigned char *result =
            (unsigned char *)k_fifo_get(&sim7600_fifo, K_MSEC(RX_TIMEOUT_MS));

        // LOG_INF("Result received");
        if (result == NULL) {
            LOG_ERR("SIM7600 RX timeout");
            return SIM7600_RX_TIMEOUT;
        }

        if (strstr(result, "DONE")) {
            // LOG_INF("DONE detected - stopping");
            if (strlen(resp_buf) + strlen(result) + 1 < 512) {
                strcat(resp_buf, result);
            }
            // LOG_INF("Startup str: %.*s", strlen(resp_buf), resp_buf);
            done_count++;
        }
        if (done_count >= 2) {
            break;
        }

        if (strlen(resp_buf) + strlen(result) + 1 < 512) {
            strcat(resp_buf, result);
        }
    }

    return SIM7600_OK;
}

SIM7600_RESULT
sim7600_init(char *mqtt_address, size_t addr_size) {
    LOG_INF("Initializing SIM7600");
    char at_output[128];

    // Check if interrupt driven UART is enabled
    _Static_assert(IS_ENABLED(CONFIG_UART_INTERRUPT_DRIVEN));

    if (!device_is_ready(dev)) {
        LOG_ERR("Cannot initialize SIM7600");
        return SIM7600_INIT_ERROR;
    }

    k_fifo_init(&sim7600_fifo);
    k_mutex_init(&sim7600_mutex);
    // k_thread_start(sim7600_worker_tid);

    // Set interrupt handler and enable it
    int ret = uart_irq_callback_set(dev, &sim7600_irq_handler);
    if (ret != 0) {
        LOG_INF("UART Callback Error: %d", ret);
        return SIM7600_INIT_ERROR;
    }
    uart_irq_rx_enable(dev);

    // TODO: Restart device on init

    // Get 2 string from fifo until timeout is triggered on startup, need to do
    // this since the modem takes quite a bit of time to startup
    LOG_INF("Parsing startup string");
    startup_parse();

    // Disable echo mode
    sim7600_send_at(ATE, sizeof(ATE), at_output, sizeof(at_output),
                    sim7600_resp_normal);

    k_sleep(K_MSEC(4000));

    // Check status of modem
    sim7600_send_at("AT+COPS?\r", sizeof("AT+COPS?\r"), at_output,
                    sizeof(at_output), sim7600_resp_normal);
    sim7600_send_at("AT+CSQ\r", sizeof("AT+CSQ\r"), at_output,
                    sizeof(at_output), sim7600_resp_normal);

    // TODO: Set up MQTT connection and set the modem to correct mode
    sim7600_send_at(AT_MQTTSTART, sizeof(AT_MQTTSTART), at_output,
                    sizeof(at_output), sim7600_resp_after_status);

    // Get MQTT client index (AT_MQTTACCQ)
    {
        char at_mqttaccq[128] = {0};
        char clientName[] = "testClient";

        int size = snprintf(at_mqttaccq, sizeof(at_mqttaccq), AT_MQTTACCQ, 0,
                            sizeof(clientName), clientName);

        sim7600_send_at(at_mqttaccq, strlen(at_mqttaccq), at_output,
                        sizeof(at_output), sim7600_resp_normal);
    }

    // Connect client to server (AT_MQTTCONNECT)
    {
        char at_mqttconnect[128] = {0};
        int size = snprintf(at_mqttconnect, sizeof(at_mqttconnect),
                            AT_MQTTCONNECT, 0, addr_size, mqtt_address, 60, 1);

        sim7600_send_at(at_mqttconnect, strlen(at_mqttconnect), at_output,
                        sizeof(at_output), sim7600_resp_after_status);

        LOG_INF("MQTT connect result: %.*s", strlen(at_output), at_output);
    }

    LOG_INF("Initializing SIM7600 done");
    return SIM7600_OK;
}

SIM7600_RESULT sim7600_close() {
    char at_result[256] = {0};

    {
        char mqtt_disconnect[30] = {0};

        int size = snprintf(mqtt_disconnect, sizeof(mqtt_disconnect),
                            AT_MQTTDIS, 0, 120);

        sim7600_send_at(mqtt_disconnect, strlen(mqtt_disconnect), at_result,
                        sizeof(at_result), sim7600_resp_normal);
        LOG_INF("Disconnect client result: %.*s", strlen(at_result), at_result);
    }

    {
        char mqtt_rel[30] = {0};

        int size = snprintf(mqtt_rel, sizeof(mqtt_rel), AT_MQTTREL, 0);

        sim7600_send_at(mqtt_rel, strlen(mqtt_rel), at_result,
                        sizeof(at_result), sim7600_resp_normal);
        LOG_INF("Release client result: %.*s", strlen(at_result), at_result);
    }

    sim7600_send_at(AT_MQTTSTOP, sizeof(AT_MQTTSTOP), at_result,
                    sizeof(at_result), sim7600_resp_normal);
    LOG_INF("Stop MQTT service result: %.*s", strlen(at_result), at_result);

    return SIM7600_OK;
}

static SIM7600_RESULT normal_parse(char *output, size_t size_out) {
    bool parsing = true;
    char resp_buf[128] = {0};
    resp_buf[0] = '\0';
    while (parsing) {
        unsigned char *result =
            (unsigned char *)k_fifo_get(&sim7600_fifo, K_MSEC(RX_TIMEOUT_MS));

        if (result == NULL) {
            LOG_ERR("SIM7600 RX timeout");
            return SIM7600_RX_TIMEOUT;
        }

        // LOG_INF("Parsing: %.*s", strlen(result), result);

        if (strcmp(result, "OK") == 0 || strcmp(result, "ERROR") == 0) {
            // LOG_INF("OK/ERROR detected on %.*s - stopping", strlen(result),
            //  result);
            parsing = false;
        }

        if (strlen(resp_buf) + strlen(result) + 1 < 512) {
            strcat(resp_buf, result);
            // LOG_INF("strcat: %.*s", strlen(resp_buf), resp_buf);
        }
    }

    strncpy(output, resp_buf, size_out);
    return SIM7600_OK;
}

static SIM7600_RESULT after_status_parse(char *output, size_t size_out) {
    bool parsing = true;
    char resp_buf[256] = {0};
    resp_buf[0] = '\0';
    while (parsing) {
        unsigned char *result =
            (unsigned char *)k_fifo_get(&sim7600_fifo, K_MSEC(RX_TIMEOUT_MS));

        if (result == NULL) {
            LOG_ERR("SIM7600 RX timeout");
            return SIM7600_RX_TIMEOUT;
        }

        // LOG_INF("Parsing: %.*s", strlen(result), result);

        if (strcmp(result, "OK") == 0) {
            // LOG_INF("OK/ERROR detected - stopping after another message");

            if (strlen(resp_buf) + strlen(result) + 1 < 512) {
                strcat(resp_buf, result);
            }

            unsigned char *result = (unsigned char *)k_fifo_get(
                &sim7600_fifo, K_MSEC(RX_TIMEOUT_MS));

            if (result == NULL) {
                LOG_ERR("SIM7600 RX timeout");
                return SIM7600_RX_TIMEOUT;
            }

            if (strlen(resp_buf) + strlen(result) + 1 < 512) {
                strcat(resp_buf, result);
            }
            break;
        } else {
            if (strlen(resp_buf) + strlen(result) + 1 < 512) {
                strcat(resp_buf, result);
            }
        }
    }

    strncpy(output, resp_buf, size_out);
    return SIM7600_OK;
}

// Implement parsing for various AT commands, can use same parser for most
// commands except MQTT connect/pub/sub
static SIM7600_RESULT parse_response(sim7600_resp_type resp_type, char *output,
                                     size_t size_out) {
    switch (resp_type) {
    case sim7600_resp_after_status:
        return after_status_parse(output, size_out);
    default:
        return normal_parse(output, size_out);
    }
}

// 1 message at a time, no need for another thread?, just send the thing
// and parse output here. The command sent is already known so it would just be
// a big switch.
// Edit: Might actually need another thread for detecting MQTT's published
// message otherwise there's no way to know if there's anything in the buffer.
SIM7600_RESULT sim7600_send_at(char *cmd, size_t size_cmd, char *output,
                               size_t size_out, sim7600_resp_type resp_type) {
    k_mutex_lock(&sim7600_mutex, K_FOREVER);
    LOG_INF("Sent AT command: %.*s", strlen(cmd), cmd);
    for (size_t i = 0; i < size_cmd; ++i) {
        uart_poll_out(dev, cmd[i]);
    }

    parse_response(resp_type, output, size_out);
    k_mutex_unlock(&sim7600_mutex);

    LOG_INF("Result: %.*s", strlen(output), output);
    return SIM7600_OK;
}

static SIM7600_RESULT sim7600_send_at_with_message(char *cmd, size_t size_cmd,
                                                   uint8_t *topic,
                                                   size_t size_topic,
                                                   char *output,
                                                   size_t size_output) {
    k_mutex_lock(&sim7600_mutex, K_FOREVER);
    for (int i = 0; i < size_cmd; i++)
        uart_poll_out(dev, cmd[i]);
    k_mutex_unlock(&sim7600_mutex);

    while (true) {
        unsigned char *result =
            (unsigned char *)k_fifo_get(&sim7600_fifo, K_MSEC(RX_TIMEOUT_MS));
        if (result == NULL) {
            return SIM7600_RESP_NULL;
        }

        if (result[0] == '>') {
            break;
        }
    }

    k_mutex_lock(&sim7600_mutex, K_FOREVER);
    for (int i = 0; i < size_topic; i++)
        uart_poll_out(dev, topic[i]);

    parse_response(sim7600_resp_normal, output, sizeof(output));
    k_mutex_unlock(&sim7600_mutex);
    return SIM7600_OK;
}

SIM7600_RESULT sim7600_set_topic_mqtt(char *topic, size_t size_topic) {
    char output[256] = {0};
    char at_cmd[256] = {0};
    SIM7600_RESULT res;

    // Set topic (AT_CMQTTTOPIC)
    int size =
        snprintf(at_cmd, sizeof(at_cmd), AT_MQTTTOPIC, 0, size_topic - 1);
    res = sim7600_send_at_with_message(at_cmd, size, topic, size_topic, output,
                                       sizeof(output));

    if (res != SIM7600_OK) {
        LOG_ERR("Error setting topic: %d", res);
    }
    LOG_INF("Set MQTT Topic response: %.*s", strlen(output), output);
    return res;
}

SIM7600_RESULT sim7600_publish_mqtt(uint8_t *payload, size_t size_payload) {
    char output[256] = {0};
    char at_cmd[256] = {0};
    SIM7600_RESULT res;

    // Set payload (AT_CMQTTPAYLOAD)
    {
        int size =
            snprintf(at_cmd, sizeof(at_cmd), AT_MQTTPAYLOAD, 0, size_payload);

        res = sim7600_send_at_with_message(at_cmd, size, payload, size_payload,
                                           output, sizeof(output));
    }
    if (res != SIM7600_OK) {
        return res;
    }

    // Publish (AT_CMQTTPUB)
    {
        int size = snprintf(at_cmd, sizeof(at_cmd), AT_MQTTPUB, 0, 1, 60);

        res = sim7600_send_at(at_cmd, size, output, sizeof(output),
                              sim7600_resp_after_status);
    }
    if (res != SIM7600_OK) {
        return res;
    }

    LOG_INF("MQTT Publish response: %.*s", strlen(output), output);
    return SIM7600_OK;
}

// The modem reset topic every time message is sent, therefore the driver needs
// to set topic everytime message is sent
SIM7600_RESULT sim7600_set_topic_publish_mqtt(char *topic, size_t size_topic,
                                              uint8_t *payload,
                                              size_t size_payload) {
    // TODO: Add structure for using AT commands to send request
    LOG_INF("Sending MQTT packet");
    char output[256] = {0};
    char at_cmd[256] = {0};
    SIM7600_RESULT res;

    res = sim7600_set_topic_mqtt(topic, size_topic);
    if (res != SIM7600_OK) {
        return res;
    }

    // Set payload (AT_CMQTTPAYLOAD)
    {
        int size =
            snprintf(at_cmd, sizeof(at_cmd), AT_MQTTPAYLOAD, 0, size_payload);

        res = sim7600_send_at_with_message(at_cmd, size, payload, size_payload,
                                           output, sizeof(output));
    }
    if (res != SIM7600_OK) {
        return res;
    }

    // Publish (AT_CMQTTPUB)
    {
        int size = snprintf(at_cmd, sizeof(at_cmd), AT_MQTTPUB, 0, 1, 60);

        res = sim7600_send_at(at_cmd, size, output, sizeof(output),
                              sim7600_resp_after_status);
    }
    if (res != SIM7600_OK) {
        return res;
    }

    LOG_INF("MQTT Publish response: %.*s", strlen(output), output);
    return SIM7600_OK;
}
