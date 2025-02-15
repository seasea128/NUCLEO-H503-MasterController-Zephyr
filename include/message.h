#include "Protobuf-FYP/proto/data.pb.h"
#include <stdint.h>

typedef enum message_type_e {
    message_undefined = 0,
    message_measurement = 1,
    message_new_session = 2,
} message_type;

typedef struct {
    void *reserved;
    message_type msg_type;
    union {
        controllerMessage_Measurement measurement;
        int32_t session_id;
    } data;
} message;

message message_init();
