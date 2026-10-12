// SPDX-License-Identifier: Apache-2.0
#ifndef RECEIVER_USB_XINPUT_H
#define RECEIVER_USB_XINPUT_H
#include "bridge.h"
#include "config_protocol.h"
config_protocol_t *usb_xinput_config(void);
void usb_xinput_init(bridge_t *state);
void usb_xinput_service(uint32_t now);
#endif
