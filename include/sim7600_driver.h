#include <stddef.h>
#include <stdint.h>

typedef enum SIM7600_RESULT_E {
    SIM7600_OK,
    SIM7600_INIT_ERROR,
    SIM7600_RX_TIMEOUT
} SIM7600_RESULT;

SIM7600_RESULT sim7600_send_at(char *cmd, size_t size_cmd, char *output,
                               size_t size_out);

SIM7600_RESULT sim7600_send_mqtt(uint8_t *buffer, size_t size);

SIM7600_RESULT sim7600_close();

SIM7600_RESULT
sim7600_init(char *mqtt_address, size_t addr_size);
