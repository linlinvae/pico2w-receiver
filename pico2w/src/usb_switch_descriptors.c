// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2021 Jason Skuby (mytechtoybox.com)
/* Switch wired HID protocol layout adapted from GP2040-CE SwitchDescriptors.h.
 * Compatibility identity 0F0D:0092 is used by this established emulation profile;
 * this is a local project, not an official HORI product. See NOTICE.md. */
#include "usb_switch_descriptors.h"
#include "tusb.h"
#include <string.h>
static const tusb_desc_device_t device = {
    .bLength = sizeof(tusb_desc_device_t), .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200, .bMaxPacketSize0 = 64, .idVendor = 0x0f0d,
    .idProduct = 0x0092, .bcdDevice = 0x0100, .iManufacturer = 1,
    .iProduct = 2, .iSerialNumber = 0, .bNumConfigurations = 1,
};
const uint8_t switch_configuration[SWITCH_CONFIG_SIZE] = {
    9,2,SWITCH_CONFIG_SIZE,0,1,1,0,0x80,125, /* MCU + radio, 250 mA */
    9,4,0,0,2,3,0,0,0,
    9,0x21,0x11,0x01,0,1,0x22,SWITCH_HID_DESCRIPTOR_SIZE,0,
    7,5,SWITCH_EP_OUT,3,64,0,1,
    7,5,SWITCH_EP_IN,3,64,0,1,
};
uint8_t const *tud_descriptor_device_cb(void) { return (const uint8_t *)&device; }
uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    return index ? NULL : switch_configuration;
}
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    static const uint8_t report[] = {
        0x05,0x01,0x09,0x05,0xa1,0x01,
        0x15,0,0x25,1,0x35,0,0x45,1,0x75,1,0x95,16,
        0x05,9,0x19,1,0x29,16,0x81,2,
        0x05,1,0x25,7,0x46,0x3b,1,0x75,4,0x95,1,
        0x65,0x14,0x09,0x39,0x81,0x42,
        0x65,0,0x95,1,0x81,1,
        0x26,0xff,0,0x46,0xff,0,0x09,0x30,0x09,0x31,
        0x09,0x32,0x09,0x35,0x75,8,0x95,4,0x81,2,
        0x06,0,0xff,0x09,0x20,0x95,1,0x81,2,
        0x0a,0x21,0x26,0x95,8,0x91,2,0xc0,
    };
    _Static_assert(sizeof(report) == SWITCH_HID_DESCRIPTOR_SIZE, "Switch HID length");
    return instance ? NULL : report;
}
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    static uint16_t text[64];
    if (!index) { text[0] = 0x0304; text[1] = 0x0409; return text; }
    const char *s;
    if (index == 1) s = "Local Receiver Project";
    else if (index == 2) s = "Pico 2 W Switch R1";
    else return NULL;
    unsigned n = (unsigned)strlen(s);
    text[0] = (uint16_t)(0x0300 | (n * 2 + 2));
    for (unsigned i = 0; i < n; ++i) text[i + 1] = (uint8_t)s[i];
    return text;
}
