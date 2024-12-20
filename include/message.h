#include <stdint.h>

typedef struct {
    uint64_t timestamp;
    uint16_t tl;
    uint16_t tr;
    uint16_t bl;
    uint16_t br;
    uint16_t speed;

    // Location data
} message;

message message_init();
