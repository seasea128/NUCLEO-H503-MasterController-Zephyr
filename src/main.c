#include "Protobuf-FYP/proto/data.pb.h"
#include "main_state.h"
#include "upload_thread/upload_thread.h"
#include <can.h>
#include <ff.h>
#include <file_op.h>
#include <message.h>
#include <save_data_thread.h>
#include <sim7600_at_cmd.h>
#include <sim7600_driver.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sd/sd_spec.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

struct k_fifo save_data_fifo;

// CAN setup
CAN_MSGQ_DEFINE(distance_msgq, 400);
const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

const struct device *const rtc = DEVICE_DT_GET(DT_ALIAS(rtc));

static struct gpio_dt_spec button_gpio =
    GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw0), gpios, {0});

void set_rtc_from_modem() {
    char resp[256] = {0};
    {
        char CNTP[128] = {0};
        snprintf(CNTP, sizeof(CNTP), AT_CNTP_SET, "pool.ntp.org");

        int ret = sim7600_send_at(CNTP, strlen(CNTP), resp, sizeof(resp),
                                  sim7600_resp_normal);
        if (ret != SIM7600_OK) {
            LOG_ERR("Cannot send AT_CNTP_SET: %d", ret);
            return;
        }
        LOG_INF("CNTP set response: %s", resp);
    }

    int ret = sim7600_send_at(AT_CNTP, sizeof(AT_CNTP), resp, sizeof(resp),
                              sim7600_resp_after_status);
    if (ret != SIM7600_OK) {
        LOG_ERR("Cannot send AT_CNTP: %d", ret);
        return;
    }
    LOG_INF("CNTP response: %s", resp);

    ret = sim7600_send_at(AT_CCLK, sizeof(AT_CCLK), resp, sizeof(resp),
                          sim7600_resp_normal);
    if (ret != SIM7600_OK) {
        LOG_ERR("Cannot send AT_CCLK: %d", ret);
        return;
    }

    if (strstr(resp, "OK") != NULL) {
        // OK is in string
        struct rtc_time new_time;
        memset(&new_time, 0, sizeof(struct rtc_time));

        // +CCLK: “08/11/28,12:30:35+32”
        // OK
        int timezone = 0;
        sscanf(resp, "+CCLK: \"%d/%d/%d,%d:%d:%d+%d\"OK", &new_time.tm_year,
               &new_time.tm_mon, &new_time.tm_mday, &new_time.tm_hour,
               &new_time.tm_min, &new_time.tm_sec, &timezone);

        new_time.tm_year += 2000 - 1900;
        new_time.tm_mon -= 1;
        ret = rtc_set_time(rtc, &new_time);
        LOG_INF("Set time success: %s, %d", resp, ret);
    } else {
        LOG_ERR("Cannot get current time from modem: %s, %d", resp, ret);
        return;
    }
}

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

    LOG_INF("Initializing SIM7600");

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

    main_state_init(&state, &distance_msgq, NULL);
    LOG_INF("Initialized main_state");

    if (!device_is_ready(rtc)) {
        LOG_INF("RTC is not ready");
    }

    set_rtc_from_modem();

#ifdef RTC_TEST
    k_sleep(K_MSEC(4834));

    struct rtc_time tm;
    ret = rtc_get_time(rtc, &tm);
    if (ret < 0) {
        LOG_ERR("Cannot get time from RTC: %d", ret);
    } else {
        LOG_INF("RTC date and time: %04d-%02d-%02d %02d:%02d:%02d.%05d",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
                tm.tm_min, tm.tm_sec, tm.tm_nsec);
    }
#endif

    LOG_INF("Button port: %s pin: %d", button_gpio.port->name, button_gpio.pin);

    bool button_state = false;

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
