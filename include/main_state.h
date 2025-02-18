#ifndef MAIN_STATE_H_
#define MAIN_STATE_H_
#include "Protobuf-FYP/proto/data.pb.h"
#include <zephyr/drivers/can.h>
#include <zephyr/fs/fs.h>

enum main_state {
    MAIN_STATE_DISK_UNMOUNTED = 0,
    MAIN_STATE_DISK_MOUNTED,
    MAIN_STATE_NEW_SESSION,
    MAIN_STATE_RECORD_DATA
};

typedef struct main_state_s {
    enum main_state state;
    struct k_msgq *can_msgq;
    struct can_frame can_message;
    struct fs_file_t file;
    uint16_t session_id;
    controllerMessage_DataPoints dataPoints;
    controllerMessage_Measurement measurement;
} main_state;

void main_state_init(main_state *state, struct k_msgq *can_msgq);
void main_state_execute(main_state *state);

#endif // MAIN_STATE_H_
