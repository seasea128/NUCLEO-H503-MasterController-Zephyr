#ifndef SAVE_DATA_THREAD
#define SAVE_DATA_THREAD
#include "Protobuf-FYP/proto/data.pb.h"
#include <zephyr/fs/fs.h>

void save_data_thread();

void write_data_points(controllerMessage_DataPoints *dataPoints,
                       struct fs_file_t *file);

void write_session(controllerMessage_Session *session, struct fs_file_t *file);

#endif
