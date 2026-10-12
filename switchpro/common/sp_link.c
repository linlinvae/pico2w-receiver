#include "sp_link.h"
#include <string.h>
static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | (uint16_t)p[1] << 8); }
static void put16(uint8_t *p, uint16_t n) { p[0] = (uint8_t)n; p[1] = (uint8_t)(n >> 8); }
uint16_t sp_crc16(const uint8_t *data, size_t size) {
    uint16_t crc = 0xffff;
    for (size_t i = 0; i < size; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (uint16_t)((crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1);
    }
    return crc;
}
void sp_encode(const sp_frame_t *frame, uint8_t wire[24]) {
    wire[0] = 0x53; wire[1] = 0x50; wire[2] = 1; wire[3] = frame->type;
    put16(wire + 4, frame->seq); memcpy(wire + 6, frame->data, 16);
    put16(wire + 22, sp_crc16(wire, 22));
}
bool sp_decode_byte(sp_decoder_t *d, uint8_t byte, sp_frame_t *f) {
    d->bytes[d->used++] = byte;
    while (d->used && (d->bytes[0] != 0x53 ||
           (d->used > 1 && d->bytes[1] != 0x50))) {
        memmove(d->bytes, d->bytes + 1, --d->used);
    }
    if (d->used < 24) return false;
    bool valid = d->bytes[2] == 1 && d->bytes[3] >= SP_INPUT &&
                 d->bytes[3] <= SP_ACK &&
                 get16(d->bytes + 22) == sp_crc16(d->bytes, 22);
    if (valid) {
        f->type = d->bytes[3]; f->seq = get16(d->bytes + 4);
        memcpy(f->data, d->bytes + 6, 16); d->used = 0;
    } else {
        /* Slide one byte: a valid header may be inside a damaged frame. */
        memmove(d->bytes, d->bytes + 1, 23); d->used = 23;
    }
    return valid;
}
void sp_pack_input(const sp_input_t *in, uint8_t p[16]) {
    memset(p, 0, 16); p[0] = in->connected; p[1] = in->battery;
    put16(p + 2, in->buttons);
    for (unsigned i = 0; i < 4; ++i) put16(p + 4 + 2 * i, (uint16_t)in->axis[i]);
    p[12] = in->lt; p[13] = in->rt;
}
bool sp_unpack_input(const uint8_t p[16], sp_input_t *in) {
    if (p[0] > 1 || (p[1] > 100 && p[1] != 255) || p[14] || p[15]) return false;
    memset(in, 0, sizeof(*in)); in->connected = p[0]; in->battery = p[1];
    if (!in->connected) return true;
    in->buttons = get16(p + 2) & 0xf7ff; /* exclude reserved XUSB bits */
    for (unsigned i = 0; i < 4; ++i) in->axis[i] = (int16_t)get16(p + 4 + 2 * i);
    in->lt = p[12]; in->rt = p[13]; return true;
}
void sp_xinput_report(const sp_input_t *in, uint8_t out[20]) {
    memset(out, 0, 20); out[1] = 20;
    if (!in->connected) return;
    put16(out + 2, in->buttons & 0xf7ff); out[4] = in->lt; out[5] = in->rt;
    for (unsigned i = 0; i < 4; ++i) put16(out + 6 + 2 * i, (uint16_t)in->axis[i]);
}
int16_t sp_scale_axis(int32_t v, bool invert) {
    if (v < -512) v = -512;
    if (v > 511) v = 511;
    if (invert) return (int16_t)(v < 0 ? -v * 32767 / 512 : -v * 32768 / 511);
    return (int16_t)(v < 0 ? v * 64 : v * 32767 / 511);
}
void sp_map_gamepad(uint16_t buttons, uint8_t dpad, uint8_t misc,
                    const int32_t axes[4], uint8_t battery, bool labels, sp_input_t *in) {
    /* Bluepad32's default is positional Xbox layout, already normalizing Switch.
     * Bits: south/east/west/north, L/R shoulders, ZL/ZR, L/R stick clicks. */
    static const uint16_t map[10] = {0x1000,0x2000,0x4000,0x8000,0x0100,0x0200,0,0,0x0040,0x0080};
    memset(in, 0, sizeof(*in)); in->connected = true;
    in->battery = battery ? (uint8_t)((battery - 1U) * 100U / 254U) : 255;
    if (labels) {
        uint16_t face = buttons & 15;
        buttons = (buttons & ~15U) | ((face & 1) << 1) | ((face & 2) >> 1) |
                  ((face & 4) << 1) | ((face & 8) >> 1);
    }
    for (unsigned i = 0; i < 10; ++i) if (buttons & (1U << i)) in->buttons |= map[i];
    if (dpad & 1) in->buttons |= 1;
    if (dpad & 2) in->buttons |= 2;
    if (dpad & 4) in->buttons |= 8;
    if (dpad & 8) in->buttons |= 4;
    if (misc & 1) in->buttons |= 0x0400; /* Home -> Guide */
    if (misc & 2) in->buttons |= 0x0020; /* Minus -> Back */
    if (misc & 4) in->buttons |= 0x0010; /* Plus -> Start */
    in->lt = (buttons & (1U << 6)) ? 255 : 0;
    in->rt = (buttons & (1U << 7)) ? 255 : 0;
    for (unsigned i = 0; i < 4; ++i) in->axis[i] = sp_scale_axis(axes[i], i & 1);
}
