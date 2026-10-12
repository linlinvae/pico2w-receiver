#include "sp_bridge.h"
#include <string.h>
static void release(sp_bridge_t *s) {
    memset(&s->input, 0, sizeof(s->input)); s->input.battery = 255;
    s->neutral_pending = true; s->input_pending = false;
    s->left = s->right = 0; s->stop_pending = true; s->dirty = true;
    /* Cancel old nonzero commands so reconnect cannot replay stale rumble. */
    s->inflight = false;
}
void sp_bridge_init(sp_bridge_t *s) { memset(s, 0, sizeof(*s)); release(s); }
void sp_bridge_receive(sp_bridge_t *s, const sp_frame_t *f, uint32_t now) {
    if (f->type == SP_INPUT) {
        sp_input_t in;
        if (!sp_unpack_input(f->data, &in)) return;
        if ((!s->link || s->input.connected) && !in.connected) release(s);
        s->input = in; s->link = true; s->last_rx = now;
        if (in.connected) s->input_pending = true;
    } else if (f->type == SP_ACK && s->inflight && f->seq == s->command.seq && f->data[0] == 0) {
        s->inflight = false;
        if (!s->command.data[0] && !s->command.data[1]) s->stop_pending = false;
    }
}
void sp_bridge_poll(sp_bridge_t *s, uint32_t now) {
    if (s->link && (uint32_t)(now - s->last_rx) >= SP_LINK_TIMEOUT_MS) {
        release(s); s->link = false;
    }
}
void sp_bridge_rumble(sp_bridge_t *s, uint8_t l, uint8_t r) {
    if ((l || r) && (!s->link || !s->input.connected)) return;
    s->left = l; s->right = r; s->dirty = true;
    if (!l && !r) {
        s->stop_pending = true;
        /* A stop preempts an outstanding nonzero rumble, preserving its control op. */
        if (s->inflight && (s->command.data[0] || s->command.data[1])) {
            if (!s->operation) s->operation = s->command.data[2];
            s->inflight = false;
        }
    }
}
void sp_bridge_control(sp_bridge_t *s, uint8_t op) {
    if (op == SP_OP_FORGET || !s->operation) s->operation = op;
    if (op == SP_OP_FORGET) sp_bridge_rumble(s, 0, 0);
    s->dirty = true;
}
bool sp_bridge_tx(sp_bridge_t *s, uint32_t now, sp_frame_t *out) {
    if (!s->link) return false;
    uint32_t elapsed = now - s->last_tx;
    if (s->inflight) {
        if (elapsed < SP_COMMAND_INTERVAL_MS) return false;
        s->last_tx = now; *out = s->command; return true;
    }
    if ((!s->dirty || elapsed < 10) && elapsed < SP_COMMAND_INTERVAL_MS) return false;
    memset(&s->command, 0, sizeof(s->command)); s->command.type = SP_COMMAND;
    s->command.seq = ++s->next_seq;
    s->command.data[0] = s->stop_pending ? 0 : s->left;
    s->command.data[1] = s->stop_pending ? 0 : s->right;
    s->command.data[2] = s->operation;
    s->operation = SP_OP_NONE;
    s->dirty = s->stop_pending && (s->left || s->right);
    s->inflight = true; s->last_tx = now; *out = s->command; return true;
}
