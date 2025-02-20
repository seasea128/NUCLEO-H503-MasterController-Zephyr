#include <stddef.h>
#include <stdint.h>

#define BUFFER_SIZE 256

typedef enum SIM7600_RESULT_E {
    SIM7600_OK,
    SIM7600_INIT_ERROR,
    SIM7600_RX_TIMEOUT,
    SIM7600_RESP_NULL
} SIM7600_RESULT;

typedef struct sim7600_msgq_item_s {
    char msg[BUFFER_SIZE];
} sim7600_msgq_item;

typedef enum sim7600_resp_type_e {
    sim7600_resp_undefined = 0,
    sim7600_resp_normal = 1,
    sim7600_resp_after_status = 2
} sim7600_resp_type;

SIM7600_RESULT sim7600_send_at(char *cmd, size_t size_cmd, char *output,
                               size_t size_out, sim7600_resp_type resp_type);

SIM7600_RESULT sim7600_publish_mqtt(uint8_t *payload, size_t size_payload);

SIM7600_RESULT sim7600_set_topic_publish_mqtt(char *topic, size_t size_topic,
                                              uint8_t *payload,
                                              size_t size_payload);

SIM7600_RESULT sim7600_set_topic_mqtt(char *topic, size_t size_topic);

SIM7600_RESULT sim7600_close();

SIM7600_RESULT
sim7600_init(char *mqtt_address, size_t addr_size);
