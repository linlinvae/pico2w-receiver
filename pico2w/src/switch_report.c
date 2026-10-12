// SPDX-License-Identifier: Apache-2.0
#include "switch_report.h"
#include <string.h>
static int32_t signed_axis(const uint8_t *p) {
    uint32_t value = (uint32_t)p[0] | (uint32_t)p[1] << 8;
    return value >= 32768 ? (int32_t)value - 65536 : (int32_t)value;
}
static uint8_t axis8(const uint8_t *p, bool invert) {
    int32_t value = signed_axis(p);
    if (invert) value = -value;
    if (value > 32767) value = 32767;
    return (uint8_t)((value + 32768) / 256);
}
void switch_report(const bridge_t *b, uint8_t out[SWITCH_REPORT_SIZE]) {
    uint8_t common[BRIDGE_REPORT_SIZE];
    bridge_report(b, common);
    uint16_t buttons = (uint16_t)(common[2] | (uint16_t)common[3] << 8);
    /* XInput A/B/X/Y -> Switch B/A/Y/X (south/east/west/north).
     * Bridge has already applied user remapping and macro output. */
    static const uint16_t from[] = {0x4000,0x1000,0x2000,0x8000,
        0x0100,0x0200,0x0020,0x0010,0x0040,0x0080,0x0400};
    static const uint16_t to[] = {1,2,4,8,16,32,256,512,1024,2048,4096};
    uint16_t result = 0;
    for (unsigned i = 0; i < sizeof(from) / sizeof(from[0]); ++i)
        if (buttons & from[i]) result |= to[i];
    if (common[4] >= 128) result |= 0x0040;
    if (common[5] >= 128) result |= 0x0080;
    if (b->usb_active && b->kind != PAD_NONE && !b->force_neutral &&
        b->have_input && (b->latest.misc & 8)) result |= 0x2000;
    out[0] = (uint8_t)result; out[1] = (uint8_t)(result >> 8);
    /* Cancel opposing directions independently, keeping the other axis. */
    bool up = (buttons & 3) == 1, down = (buttons & 3) == 2;
    bool left = (buttons & 12) == 4, right = (buttons & 12) == 8;
    out[2] = up ? (left ? 7 : right ? 1 : 0) :
        down ? (left ? 5 : right ? 3 : 4) : left ? 6 : right ? 2 : 8;
    out[3] = axis8(common + 6, false);
    out[4] = axis8(common + 8, true); /* Switch Y increases downward. */
    out[5] = axis8(common + 10, false);
    out[6] = axis8(common + 12, true);
    out[7] = 0;
}
