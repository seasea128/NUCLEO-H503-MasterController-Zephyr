#ifndef UPLOAD_STATE_H_
#define UPLOAD_STATE_H_
#include "Protobuf-FYP/proto/data.pb.h"
#include <stdio.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>

enum upload_state {
    UPLOAD_STATE_DISCONNECTED = 0,
    UPLOAD_STATE_CONNECTED_SEND,
    UPLOAD_STATE_CONNECTED_RECEIVING,
    UPLOAD_STATE_CONNECTED_READING_FILE,
    UPLOAD_STATE_CONNECTED_WAITING_DATA
};

typedef struct upload_state_s {
    char data[controllerMessage_Packet_size];
    enum upload_state state;
    size_t upload_file_offset;
    size_t data_index;
    int current_session_id;

} upload_state;

void upload_state_init(upload_state *state, struct k_msgq *upload_data_msgq);

void upload_state_execute(upload_state *state, struct fs_file_t *file);

#endif // UPLOAD_STATE_H_
