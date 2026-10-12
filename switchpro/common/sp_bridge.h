#ifndef SP_BRIDGE_H
#define SP_BRIDGE_H
#include "sp_link.h"
typedef struct {
    sp_input_t input;
    uint32_t last_rx, last_tx;
    uint16_t next_seq;
    uint8_t left, right, operation;
    bool link, neutral_pending, input_pending, dirty, stop_pending, inflight;
    sp_frame_t command;
} sp_bridge_t;
void sp_bridge_init(sp_bridge_t *bridge);
void sp_bridge_receive(sp_bridge_t *bridge, const sp_frame_t *frame, uint32_t now);
void sp_bridge_poll(sp_bridge_t *bridge, uint32_t now);
void sp_bridge_rumble(sp_bridge_t *bridge, uint8_t left, uint8_t right);
void sp_bridge_control(sp_bridge_t *bridge, uint8_t operation);
/* Call only when the transport can accept a complete frame. Retries are idempotent. */
bool sp_bridge_tx(sp_bridge_t *bridge, uint32_t now, sp_frame_t *frame);
#endif
