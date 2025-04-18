#ifndef MAIN_STATE_H_
#define MAIN_STATE_H_
#include "Protobuf-FYP/proto/data.pb.h"
#include "upload_state.h"
#include <zephyr/drivers/can.h>
#include <zephyr/fs/fs.h>

enum main_state {
    MAIN_STATE_DISK_UNMOUNTED = 0,
    MAIN_STATE_DISK_MOUNTED,
    MAIN_STATE_NEW_SESSION,
    MAIN_STATE_RECORD_DATA,
};

typedef struct main_state_s {
    controllerMessage_DataPoints dataPoints;
    controllerMessage_Measurement measurement;
    upload_state upload;
    struct can_frame can_message;
    struct fs_file_t file;
    struct k_msgq *can_msgq;
    struct k_msgq *upload_msgq;
    enum main_state state;
    uint16_t session_id;
    size_t upload_data_offset;
} main_state;

void main_state_init(main_state *state, struct k_msgq *can_msgq,
                     struct k_msgq *upload_msgq);
void main_state_execute(main_state *state);

#endif // MAIN_STATE_H_
