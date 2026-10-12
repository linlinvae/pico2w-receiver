#include "sp_endpoint.h"
#include <string.h>
bool sp_endpoint_accept(sp_endpoint_t *s, const sp_frame_t *f, uint32_t now, uint8_t *op) {
    if (f->type != SP_COMMAND || f->data[2] > SP_OP_FORGET) return false;
    for (unsigned i = 3; i < 16; ++i) if (f->data[i]) return false;
    bool duplicate = s->seen && f->seq == s->previous.seq && !memcmp(f->data, s->previous.data, 16);
    *op = duplicate ? SP_OP_NONE : f->data[2];
    s->left = f->data[0]; s->right = f->data[1]; s->last_command = now;
    s->previous = *f; s->seen = s->live = true; return true;
}
bool sp_endpoint_expire(sp_endpoint_t *s, uint32_t now) {
    if (!s->live || (uint32_t)(now - s->last_command) < SP_COMMAND_WATCHDOG_MS) return false;
    s->live = false; s->left = s->right = 0; return true;
}
