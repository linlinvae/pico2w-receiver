#ifndef SP_ENDPOINT_H
#define SP_ENDPOINT_H
#include "sp_link.h"
typedef struct {
    sp_frame_t previous;
    uint32_t last_command;
    uint8_t left, right;
    bool seen, live;
} sp_endpoint_t;
/* Operation returned once per command; rumble may refresh on an ACK retry. */
bool sp_endpoint_accept(sp_endpoint_t *endpoint, const sp_frame_t *frame,
                        uint32_t now, uint8_t *operation);
bool sp_endpoint_expire(sp_endpoint_t *endpoint, uint32_t now);
#endif
