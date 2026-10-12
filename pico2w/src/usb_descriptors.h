// SPDX-License-Identifier: Apache-2.0
#ifndef RECEIVER_USB_DESCRIPTORS_H
#define RECEIVER_USB_DESCRIPTORS_H
#include <stdint.h>
#define XINPUT_EP_IN 0x83
#define XINPUT_EP_OUT 0x03
#define XINPUT_VENDOR_CODE 0x20
#define XINPUT_CONFIG_SIZE 74
#define STATUS_HID_DESCRIPTOR_SIZE 39
extern const uint8_t xinput_configuration[XINPUT_CONFIG_SIZE];
extern const uint8_t xinput_compatible_id[40];
#endif
