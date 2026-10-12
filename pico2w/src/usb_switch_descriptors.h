// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2021 Jason Skuby (mytechtoybox.com)
#ifndef RECEIVER_USB_SWITCH_DESCRIPTORS_H
#define RECEIVER_USB_SWITCH_DESCRIPTORS_H
#include <stdint.h>
#define SWITCH_CONFIG_SIZE 41
#define SWITCH_HID_DESCRIPTOR_SIZE 86
#define SWITCH_EP_IN 0x81
#define SWITCH_EP_OUT 0x02
extern const uint8_t switch_configuration[SWITCH_CONFIG_SIZE];
#endif
