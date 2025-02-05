#include "zephyr/kernel.h"
#include <stddef.h>
#include <stdint.h>

typedef enum SIM7600_RESULT_E {
    SIM7600_OK,
    SIM7600_INIT_ERROR,
    SIM7600_RX_TIMEOUT
} SIM7600_RESULT;

typedef struct sim7600_work_info_s {
    struct k_work work;
} sim7600_work_info;

typedef struct sim7600_msgq_item_s {
    char msg[512];
} sim7600_msgq_item;

typedef enum sim7600_resp_type_e {
    sim7600_resp_undefined = 0
} sim7600_resp_type;

typedef struct sim7600_resp_s {
    void *fifo_reserved;
    sim7600_resp_type type;
    char message[512];
} sim7600_resp;

SIM7600_RESULT
sim7600_send_at(char *cmd, size_t size_cmd, char *output, size_t size_out);

SIM7600_RESULT sim7600_send_mqtt(uint8_t *buffer, size_t size);

SIM7600_RESULT sim7600_close();

SIM7600_RESULT
sim7600_init(char *mqtt_address, size_t addr_size);
