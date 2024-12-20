#include "message.h"
#include "zephyr/types.h"
#include <assert.h>
#include <stdio.h>

const char to_string_template[] = "";
const size_t to_string_size = sizeof(to_string_template);

message message_init() {
    message msg = {};
    return msg;
}

int message_to_string(message *msg, char *buffer, const size_t buf_length) {
    return snprintf(buffer, buf_length, to_string_template, msg->tl);
}
