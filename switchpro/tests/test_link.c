#include "sp_link.h"
#include "sp_bridge.h"
#include "sp_endpoint.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
static unsigned checks, failures;
#define CHECK(c) do { ++checks; if (!(c)) { ++failures; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)
static sp_frame_t status(bool online) {
    sp_input_t in = {.connected = online, .battery = 100, .buttons = 0x1000,
        .axis = {-32768,32767,1234,-4321}, .lt = 255};
    sp_frame_t f = {.type = SP_INPUT, .seq = 15}; sp_pack_input(&in, f.data); return f;
}
static void wire_tests(void) {
    CHECK(sp_crc16((const uint8_t *)"123456789", 9) == 0x29b1);
    sp_frame_t f = status(true), out; uint8_t wire[24]; sp_encode(&f, wire);
    sp_decoder_t d = {0};
    for (unsigned i = 0; i < 23; ++i) CHECK(!sp_decode_byte(&d, wire[i], &out));
    CHECK(sp_decode_byte(&d, wire[23], &out) && out.seq == f.seq && !memcmp(out.data, f.data, 16));
    for (unsigned bad = 0; bad < 24 * 8; ++bad) {
        uint8_t broken[24]; memcpy(broken, wire, 24); broken[bad / 8] ^= 1U << (bad % 8);
        memset(&d, 0, sizeof(d)); unsigned count = 0;
        for (unsigned i = 0; i < 24; ++i) count += sp_decode_byte(&d, broken[i], &out);
        for (unsigned i = 0; i < 24; ++i) count += sp_decode_byte(&d, wire[i], &out);
        CHECK(count == 1 && out.seq == f.seq);
    }
    for (unsigned lost = 0; lost < 24; ++lost) {
        memset(&d, 0, sizeof(d)); unsigned count = 0;
        for (unsigned i = 0; i < 24; ++i) if (i != lost) count += sp_decode_byte(&d, wire[i], &out);
        for (unsigned i = 0; i < 24; ++i) count += sp_decode_byte(&d, wire[i], &out);
        CHECK(count == 1);
    }
    /* A partial frame followed by noise must not hide the next valid frame. */
    memset(&d, 0, sizeof(d));
    for (unsigned i = 0; i < 7; ++i) sp_decode_byte(&d, wire[i], &out);
    for (unsigned i = 0; i < 1000; ++i) CHECK(!sp_decode_byte(&d, (uint8_t)i, &out));
    unsigned count = 0;
    for (unsigned i = 0; i < 24; ++i) count += sp_decode_byte(&d, wire[i], &out);
    CHECK(count == 1);
    sp_input_t in; CHECK(sp_unpack_input(f.data, &in));
    CHECK(in.axis[0] == -32768 && in.axis[1] == 32767 && in.axis[3] == -4321);
    uint8_t report[20]; sp_xinput_report(&in, report);
    CHECK(report[1] == 20 && report[3] == 0x10 && report[4] == 255 && report[5] == 0);
    CHECK(report[6] == 0 && report[7] == 0x80 && report[8] == 0xff && report[9] == 0x7f);
    f.data[0] = 0; CHECK(sp_unpack_input(f.data, &in)); sp_xinput_report(&in, report);
    for (unsigned i = 2; i < 20; ++i) CHECK(report[i] == 0);
    f.data[0] = 2; CHECK(!sp_unpack_input(f.data, &in));
    f.data[0] = 1; f.data[1] = 101; CHECK(!sp_unpack_input(f.data, &in));
    f.data[1] = 255; f.data[14] = 1; CHECK(!sp_unpack_input(f.data, &in));
}
static void mapping_tests(void) {
    CHECK(sp_scale_axis(-512, false) == -32768 && sp_scale_axis(511, false) == 32767);
    CHECK(sp_scale_axis(-512, true) == 32767 && sp_scale_axis(511, true) == -32768);
    CHECK(sp_scale_axis(0, false) == 0 && sp_scale_axis(0, true) == 0);
    CHECK(sp_scale_axis(INT_MIN, false) == -32768 && sp_scale_axis(INT_MAX, true) == -32768);
    int32_t axes[4] = {-512,-512,511,511}; sp_input_t in;
    sp_map_gamepad(1, 1, 7, axes, 255, false, &in);
    CHECK(in.connected && in.battery == 100 && in.buttons == 0x1431);
    CHECK(in.axis[0] == -32768 && in.axis[1] == 32767 && in.axis[3] == -32768);
    sp_map_gamepad(1, 0, 0, axes, 0, true, &in); CHECK(in.buttons == 0x2000 && in.battery == 255);
    sp_map_gamepad(4, 0, 0, axes, 1, true, &in); CHECK(in.buttons == 0x8000 && in.battery == 0);
    sp_map_gamepad(3U << 6, 0, 8, axes, 128, false, &in);
    CHECK(in.lt == 255 && in.rt == 255 && in.buttons == 0); /* Capture has no XInput slot */
    sp_map_gamepad(0x33f, 15, 7, axes, 64, false, &in); CHECK(in.buttons == 0xf7ff);
}
static void bridge_tests(void) {
    sp_bridge_t s; sp_bridge_init(&s); sp_frame_t out;
    CHECK(s.neutral_pending && !sp_bridge_tx(&s, 100, &out));
    sp_frame_t f = status(true); sp_bridge_receive(&s, &f, 100);
    CHECK(s.link && s.input.connected && s.neutral_pending && s.input_pending);
    CHECK(sp_bridge_tx(&s, 100, &out) && out.data[0] == 0 && out.data[1] == 0);
    sp_frame_t ack = {.type = SP_ACK, .seq = out.seq + 1};
    sp_bridge_receive(&s, &ack, 101); CHECK(s.inflight);
    ack.seq = out.seq; sp_bridge_receive(&s, &ack, 102); CHECK(!s.inflight && !s.stop_pending);
    sp_bridge_rumble(&s, 100, 200); CHECK(!sp_bridge_tx(&s, 105, &out));
    CHECK(sp_bridge_tx(&s, 110, &out) && out.data[0] == 100 && out.data[1] == 200);
    uint16_t seq = out.seq;
    CHECK(!sp_bridge_tx(&s, 209, &out));
    CHECK(sp_bridge_tx(&s, 210, &out) && out.seq == seq); /* lost ACK */
    sp_bridge_rumble(&s, 0, 0); sp_bridge_rumble(&s, 30, 40);
    CHECK(sp_bridge_tx(&s, 220, &out) && out.data[0] == 0 && out.data[1] == 0);
    ack.seq = seq; sp_bridge_receive(&s, &ack, 221); CHECK(s.inflight); /* superseded ACK */
    ack.seq = out.seq; sp_bridge_receive(&s, &ack, 222);
    CHECK(sp_bridge_tx(&s, 230, &out) && out.data[0] == 30 && out.data[1] == 40);
    sp_bridge_poll(&s, 349); CHECK(s.link);
    sp_bridge_poll(&s, 350); CHECK(!s.link && !s.input.connected && s.neutral_pending && !s.inflight);
    sp_bridge_rumble(&s, 250, 250); CHECK(s.left == 0 && s.right == 0);
    f = status(false); sp_bridge_receive(&s, &f, 400);
    CHECK(sp_bridge_tx(&s, 400, &out) && out.data[0] == 0);
    ack.seq = out.seq; sp_bridge_receive(&s, &ack, 401);
    sp_bridge_control(&s, SP_OP_SCAN); sp_bridge_control(&s, SP_OP_FORGET);
    CHECK(sp_bridge_tx(&s, 410, &out) && out.data[2] == SP_OP_FORGET && out.data[0] == 0);
    /* Invalid packets cannot keep an obsolete pressed button alive. */
    sp_bridge_init(&s); f = status(true); sp_bridge_receive(&s, &f, UINT32_MAX - 100);
    f.data[14] = 1; sp_bridge_receive(&s, &f, 100);
    sp_bridge_poll(&s, 148); CHECK(s.link);
    sp_bridge_poll(&s, 149); CHECK(!s.link && s.neutral_pending);
}
static void endpoint_tests(void) {
    sp_endpoint_t s = {0}; sp_frame_t f = {.type = SP_COMMAND, .seq = 12}; uint8_t op;
    f.data[0] = 80; f.data[1] = 120; f.data[2] = SP_OP_FORGET;
    CHECK(sp_endpoint_accept(&s, &f, 100, &op) && op == SP_OP_FORGET && s.left == 80);
    CHECK(sp_endpoint_accept(&s, &f, 200, &op) && op == SP_OP_NONE);
    CHECK(!sp_endpoint_expire(&s, 499)); CHECK(sp_endpoint_expire(&s, 500) && s.left == 0 && s.right == 0);
    CHECK(!sp_endpoint_expire(&s, 1000));
    CHECK(sp_endpoint_accept(&s, &f, 1001, &op) && op == SP_OP_NONE); /* no double erase after timeout */
    f.data[5] = 1; CHECK(!sp_endpoint_accept(&s, &f, 1200, &op));
    CHECK(s.last_command == 1001);
    f.data[5] = 0; f.data[2] = 3; CHECK(!sp_endpoint_accept(&s, &f, 1200, &op));
    f.data[2] = 0; CHECK(sp_endpoint_accept(&s, &f, UINT32_MAX - 100, &op));
    CHECK(!sp_endpoint_expire(&s, 198)); CHECK(sp_endpoint_expire(&s, 199));
    /* Round trip: command, lost ACK, retry must not repeat a control action. */
    sp_bridge_t b; sp_bridge_init(&b); sp_frame_t in = status(false), tx;
    sp_bridge_receive(&b, &in, 100); sp_bridge_control(&b, SP_OP_SCAN);
    CHECK(sp_bridge_tx(&b, 100, &tx)); memset(&s, 0, sizeof(s));
    CHECK(sp_endpoint_accept(&s, &tx, 100, &op) && op == SP_OP_SCAN);
    CHECK(sp_bridge_tx(&b, 200, &tx)); CHECK(sp_endpoint_accept(&s, &tx, 200, &op) && op == SP_OP_NONE);
    sp_frame_t ack = {.type = SP_ACK, .seq = tx.seq}; sp_bridge_receive(&b, &ack, 201);
    CHECK(!b.inflight);
}
int main(void) {
    wire_tests(); mapping_tests(); bridge_tests(); endpoint_tests();
    printf("Switch Pro link: %u checks, %u failures\n", checks, failures); return failures ? 1 : 0;
}
