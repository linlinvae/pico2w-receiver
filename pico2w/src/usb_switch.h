// SPDX-License-Identifier: Apache-2.0
#ifndef RECEIVER_USB_SWITCH_H
#define RECEIVER_USB_SWITCH_H
#include "bridge.h"
#include "config_protocol.h"
#define SWITCH_USB_INTERVAL_MS 1
config_protocol_t *usb_switch_config(void);
void usb_switch_init(bridge_t *state);
void usb_switch_service(uint32_t now);
#endif
