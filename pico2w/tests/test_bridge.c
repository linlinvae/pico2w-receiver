#include "bridge.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { assert(x); ++checks; } while (0)
static int16_t get16(const uint8_t *p) { return (int16_t)((uint16_t)p[0] | (uint16_t)p[1] << 8); }
static void check_neutral(bridge_t *b) {
    uint8_t r[20]; bridge_report(b, r);
    CHECK(r[0] == 0 && r[1] == 20);
    for (unsigned i = 2; i < 20; ++i) CHECK(r[i] == 0);
}
int main(void) {
    bridge_t b; bridge_init(&b); check_neutral(&b);
    CHECK(bridge_identify(0x045e,0x0b13,true,false,true) == PAD_XSX);
    CHECK(bridge_identify(0x057e,0x2009,false,true,false) == PAD_SWITCH_PRO);
    CHECK(bridge_identify(0x18d1,0x9400,true,false,false) == PAD_STADIA);
    CHECK(bridge_identify(0x045e,0x0b13,false,false,true) == PAD_NONE);
    CHECK(bridge_identify(0x057e,0x2009,true,true,false) == PAD_NONE);
    CHECK(bridge_identify(0x045e,0x0b20,true,false,true) == PAD_NONE);
    CHECK(bridge_identify(0x045e,0x0b13,true,false,false) == PAD_NONE);
    CHECK(bridge_identify(0x057e,0x2009,false,false,false) == PAD_NONE);
    CHECK(bridge_identify(0xffff,0xffff,true,true,true) == PAD_NONE);
    for (int x = -700; x <= 700; ++x) {
        CHECK(bridge_axis(x,false) >= -32768 && bridge_axis(x,false) <= 32767);
        CHECK(bridge_axis(x,true) >= -32768 && bridge_axis(x,true) <= 32767);
        if (x > -700) {
            CHECK(bridge_axis(x,false) >= bridge_axis(x-1,false));
            CHECK(bridge_axis(x,true) <= bridge_axis(x-1,true));
        }
    }
    CHECK(bridge_axis(-512,false) == -32768 && bridge_axis(511,false) == 32767);
    CHECK(bridge_axis(-512,true) == 32767 && bridge_axis(511,true) == -32768);
    CHECK(bridge_axis(0,false) == 0 && bridge_axis(0,true) == 0);
    for (int x = -10; x <= 1040; ++x) {
        CHECK(bridge_trigger(x) <= 255);
        if (x > -10) CHECK(bridge_trigger(x) >= bridge_trigger(x-1));
    }
    CHECK(bridge_trigger(-1) == 0 && bridge_trigger(1023) == 255);
    CHECK(bridge_trigger(512) == 128);
    bridge_connect(&b, PAD_XSX); bridge_usb_active(&b, true);
    pad_input_t in = {.buttons=0x3ff,.dpad=15,.misc=15,
        .axes={-512,-512,511,511},.brake=512,.throttle=1023};
    bridge_input(&b,&in); check_neutral(&b); bridge_report_sent(&b);
    uint8_t r[20]; bridge_report(&b,r);
    CHECK((uint16_t)get16(r+2) == 0xf7ff);
    CHECK(r[4] == 128 && r[5] == 255);
    CHECK(get16(r+6) == -32768 && get16(r+8) == 32767);
    CHECK(get16(r+10) == 32767 && get16(r+12) == -32768);
    for (unsigned i = 14; i < 20; ++i) CHECK(r[i] == 0);
    for (unsigned i = 0; i < 1000; ++i) { bridge_report(&b,r); CHECK(r[2] != 0); }
    uint8_t s,w; CHECK(bridge_take_rumble(&b,false,&s,&w) && !s && !w);
    const uint8_t on[8]={0,8,0,200,80,0,0,0}, off[8]={0,8,0,0,0,0,0,0};
    CHECK(bridge_output(&b,on,8)); CHECK(bridge_take_rumble(&b,false,&s,&w) && s==200 && w==80);
    CHECK(!bridge_take_rumble(&b,false,&s,&w));
    CHECK(bridge_take_rumble(&b,true,&s,&w) && s==200 && w==80);
    CHECK(bridge_output(&b,off,8)); CHECK(bridge_output(&b,on,8));
    CHECK(bridge_take_rumble(&b,false,&s,&w) && !s && !w);
    CHECK(bridge_take_rumble(&b,false,&s,&w) && s==200 && w==80);
    CHECK(!bridge_output(&b,NULL,8)); CHECK(!bridge_output(&b,on,7));
    uint8_t bad[8]={1,8,0,255,255}; CHECK(!bridge_output(&b,bad,8));
    bad[0]=0; bad[7]=1; CHECK(!bridge_output(&b,bad,8));
    CHECK(b.strong==200 && b.weak==80);
    bridge_usb_active(&b,false); check_neutral(&b);
    CHECK(bridge_take_rumble(&b,false,&s,&w) && !s && !w);
    CHECK(bridge_output(&b,on,8) && !b.strong && !b.weak);
    bridge_usb_active(&b,true); check_neutral(&b); bridge_report_sent(&b);
    bridge_report(&b,r); CHECK(r[4] == 128);
    bridge_disconnect(&b); check_neutral(&b);
    bridge_input(&b,&in); check_neutral(&b);
    CHECK(bridge_output(&b,on,8) && !b.strong && !b.weak);
    bridge_connect(&b,PAD_SWITCH_PRO); bridge_input(&b,&in); bridge_report_sent(&b);
    bridge_report(&b,r); CHECK(r[4]==255 && r[5]==255);
    in.buttons=1; in.brake=1023; in.throttle=1023; bridge_input(&b,&in);
    bridge_report(&b,r); CHECK(r[2]==0x3f && r[3]==0x14 && !r[4] && !r[5]);
    bridge_connect(&b,PAD_STADIA); in.buttons=0xc0; in.brake=128; in.throttle=512;
    bridge_input(&b,&in); bridge_report_sent(&b); bridge_report(&b,r);
    CHECK(r[4]==32 && r[5]==128);
    printf("Bridge: %u checks passed\n",checks); return 0;
}
