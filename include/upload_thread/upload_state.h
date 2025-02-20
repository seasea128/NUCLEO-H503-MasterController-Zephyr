#ifndef UPLOAD_STATE_H_
#define UPLOAD_STATE_H_
#include <stdio.h>
#include <zephyr/fs/fs.h>

enum upload_state {
    UPLOAD_STATE_DISCONNECTED = 0,
    UPLOAD_STATE_CONNECTED,
    UPLOAD_STATE_READING_FILE,
    UPLOAD_STATE_WAITING_DATA
};

typedef struct upload_state_s {
    enum upload_state state;
    FILE *file_posix;
    struct fs_file_t file;
    struct k_msgq *upload_data_msgq;
    int current_session_id;

} upload_state;

void upload_state_init(upload_state *state, struct k_msgq *upload_data_msgq);

void upload_state_execute(upload_state *state);

#endif // UPLOAD_STATE_H_
