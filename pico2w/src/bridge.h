// SPDX-License-Identifier: Apache-2.0
#ifndef RECEIVER_BRIDGE_H
#define RECEIVER_BRIDGE_H
#include <stdbool.h>
#include <stdint.h>
#include "receiver_config.h"
#define BRIDGE_REPORT_SIZE 20
typedef enum { PAD_NONE, PAD_XSX, PAD_SWITCH_PRO, PAD_STADIA } pad_kind_t;
typedef struct {
    uint16_t buttons;
    uint8_t dpad, misc;
    int32_t axes[4], brake, throttle;
} pad_input_t;
typedef struct {
    pad_kind_t kind;
    uint8_t report[BRIDGE_REPORT_SIZE];
    uint8_t strong, weak;
    bool usb_active, force_neutral, stop_pending, rumble_dirty;
    receiver_config_t config;
    pad_input_t latest;
    bool have_input, macro_active;
    uint16_t previous_buttons;
    uint32_t clock_ms, macro_started;
} bridge_t;
void bridge_init(bridge_t *b);
void bridge_connect(bridge_t *b, pad_kind_t kind);
void bridge_disconnect(bridge_t *b);
void bridge_usb_active(bridge_t *b, bool active);
void bridge_input(bridge_t *b, const pad_input_t *input);
void bridge_tick(bridge_t *b, uint32_t now);
void bridge_apply_config(bridge_t *b, const receiver_config_t *config);
void bridge_report(const bridge_t *b, uint8_t out[BRIDGE_REPORT_SIZE]);
void bridge_report_sent(bridge_t *b);
bool bridge_output(bridge_t *b, const uint8_t *packet, unsigned len);
/* A queued stop is consumed before newer nonzero rumble. */
bool bridge_take_rumble(bridge_t *b, bool refresh, uint8_t *strong, uint8_t *weak);
int16_t bridge_axis(int32_t v, bool invert);
uint8_t bridge_trigger(int32_t v);
pad_kind_t bridge_identify(uint16_t vid, uint16_t pid, bool ble, bool switch_parser, bool xbox_parser);
#endif
