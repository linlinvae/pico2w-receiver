// SPDX-License-Identifier: Apache-2.0
#include "bridge.h"
#include <string.h>
static void neutral(uint8_t out[20]) { memset(out, 0, 20); out[1] = 20; }
static void render(bridge_t *b);
static void put16(uint8_t *p, int32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)((uint16_t)v >> 8); }
static void clear_rumble(bridge_t *b) {
    b->strong = b->weak = 0; b->stop_pending = true; b->rumble_dirty = false;
}
void bridge_init(bridge_t *b) { memset(b, 0, sizeof(*b));config_defaults(&b->config);neutral(b->report); }
void bridge_connect(bridge_t *b, pad_kind_t kind) {
    b->kind = kind; neutral(b->report); b->force_neutral = true; clear_rumble(b);
    b->have_input=b->macro_active=false;b->previous_buttons=0;
}
void bridge_disconnect(bridge_t *b) { bridge_connect(b, PAD_NONE); }
void bridge_usb_active(bridge_t *b, bool active) {
    if (b->usb_active == active) return;
    b->usb_active = active; b->force_neutral = true; clear_rumble(b);
    if(!active) { b->macro_active=false;render(b); }
}
int16_t bridge_axis(int32_t v, bool invert) {
    if (v < -512) v = -512;
    if (v > 511) v = 511;
    if (invert) return (int16_t)(v < 0 ? -v * 32767 / 512 : -v * 32768 / 511);
    return (int16_t)(v < 0 ? v * 64 : v * 32767 / 511);
}
uint8_t bridge_trigger(int32_t v) {
    if (v < 0) v = 0;
    if (v > 1023) v = 1023;
    return (uint8_t)((v * 255 + 511) / 1023);
}
static uint16_t physical_buttons(const pad_input_t *in) {
    static const uint16_t map[10] = {0x1000,0x2000,0x4000,0x8000,0x0100,0x0200,0,0,0x0040,0x0080};
    uint16_t mask = 0;
    for (unsigned i = 0; i < 10; ++i) if (in->buttons & (1U << i)) mask |= map[i];
    if (in->dpad & 1) mask |= 1;
    if (in->dpad & 2) mask |= 2;
    if (in->dpad & 4) mask |= 8;
    if (in->dpad & 8) mask |= 4;
    if (in->misc & 1) mask |= 0x0400;
    if (in->misc & 2) mask |= 0x0020;
    if (in->misc & 4) mask |= 0x0010;
    return mask;
}
static int16_t deadzone(int16_t v,uint8_t percent,bool invert) {
    int32_t mag=v<0?-(int32_t)v:v,range=v<0?32768:32767,inner=range*percent/100;
    int32_t result=mag<=inner?0:(mag-inner)*range/(range-inner);
    if(v<0)result=-result;
    if(invert)result=-result;
    if(result>32767)result=32767;
    if(result< -32768)result=-32768;
    return (int16_t)result;
}
static void render(bridge_t *b) {
    if(!b->have_input || b->kind==PAD_NONE)return;
    const pad_input_t *in=&b->latest;uint16_t raw=physical_buttons(in),mask=0;
    if(b->config.count && b->config.trigger<CONFIG_BUTTONS)raw&=(uint16_t)~config_button_bits[b->config.trigger];
    for(unsigned i=0;i<CONFIG_BUTTONS;i++)if((raw&config_button_bits[i]) && b->config.map[i]<CONFIG_BUTTONS)mask|=config_button_bits[b->config.map[i]];
    neutral(b->report);
    /* Stadia also reports digital trigger bits: never let those flatten analog. */
    if (b->kind == PAD_SWITCH_PRO) {
        b->report[4] = (in->buttons & (1U << 6)) ? 255 : 0;
        b->report[5] = (in->buttons & (1U << 7)) ? 255 : 0;
    } else {
        b->report[4] = bridge_trigger(in->brake);
        b->report[5] = bridge_trigger(in->throttle);
    }
    for (unsigned i = 0; i < 4; ++i) put16(b->report + 6 + i * 2,
        deadzone(bridge_axis(in->axes[i],i&1),b->config.deadzone[i/2],(b->config.invert&(1u<<i))!=0));
    if(b->macro_active) {
        uint32_t elapsed=b->clock_ms-b->macro_started,total=0;unsigned index=0;
        for(;index<b->config.count;index++) { total+=b->config.steps[index].duration;if(elapsed<total)break; }
        if(index==b->config.count)b->macro_active=false;
        else {
            const config_step_t *s=&b->config.steps[index];mask|=s->buttons;
            if(s->lt>b->report[4])b->report[4]=s->lt;
            if(s->rt>b->report[5])b->report[5]=s->rt;
        }
    }
    put16(b->report+2,mask);
}
void bridge_input(bridge_t *b,const pad_input_t *in) {
    if(b->kind==PAD_NONE)return;
    uint16_t raw=physical_buttons(in);
    if(b->usb_active && b->config.count && b->config.trigger<CONFIG_BUTTONS) {
        uint16_t trigger=config_button_bits[b->config.trigger];
        if((raw&trigger) && !(b->previous_buttons&trigger) && !b->macro_active) { b->macro_active=true;b->macro_started=b->clock_ms; }
    }
    b->previous_buttons=raw;b->latest=*in;b->have_input=true;render(b);
}
void bridge_tick(bridge_t *b,uint32_t now) { b->clock_ms=now;if(b->macro_active)render(b); }
void bridge_apply_config(bridge_t *b,const receiver_config_t *c) {
    if(!config_valid(c))return;
    b->config=*c;b->macro_active=false;b->force_neutral=true;clear_rumble(b);
    b->previous_buttons=b->have_input?physical_buttons(&b->latest):0;render(b);
}
void bridge_report(const bridge_t *b, uint8_t out[20]) {
    if (b->force_neutral || !b->usb_active || b->kind == PAD_NONE) neutral(out);
    else memcpy(out, b->report, 20);
}
void bridge_report_sent(bridge_t *b) { b->force_neutral = false; }
bool bridge_output(bridge_t *b, const uint8_t *p, unsigned len) {
    if (!p || len != 8 || p[0] || p[1] != 8 || p[2] || p[5] || p[6] || p[7]) return false;
    if (!b->usb_active || b->kind == PAD_NONE) return true;
    b->strong = p[3]; b->weak = p[4];
    if (!b->strong && !b->weak) { b->stop_pending = true; b->rumble_dirty = false; }
    else b->rumble_dirty = true;
    return true;
}
bool bridge_take_rumble(bridge_t *b, bool refresh, uint8_t *strong, uint8_t *weak) {
    if (b->stop_pending) {
        b->stop_pending = false; *strong = *weak = 0; return true;
    }
    if (!b->rumble_dirty && !(refresh && (b->strong || b->weak))) return false;
    b->rumble_dirty = false;
    *strong = (uint8_t)((unsigned)b->strong*b->config.rumble[0]/100);
    *weak = (uint8_t)((unsigned)b->weak*b->config.rumble[1]/100); return true;
}
pad_kind_t bridge_identify(uint16_t vid, uint16_t pid, bool ble, bool sw, bool xbox) {
    if (!ble && sw && vid == 0x057e && pid == 0x2009) return PAD_SWITCH_PRO;
    if (ble && xbox && vid == 0x045e && pid == 0x0b13) return PAD_XSX;
    if (ble && vid == 0x18d1 && pid == 0x9400) return PAD_STADIA;
    return PAD_NONE;
}
