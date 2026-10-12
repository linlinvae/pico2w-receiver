// SPDX-License-Identifier: Apache-2.0
#include "tusb.h"
#include "pico/unique_id.h"
#include "usb_descriptors.h"
#include <string.h>
/* Wired XUSB subtype 1, defined by MS-XUSBI. Same working endpoint layout as
 * this project's CH582F receiver, now with a dedicated TinyUSB class driver. */
const uint8_t xinput_configuration[XINPUT_CONFIG_SIZE] = {
    9,2,XINPUT_CONFIG_SIZE,0,2,1,0,0x80,125, /* 250 mA configured budget for radio + MCU */
    9,4,0,0,2,0xff,0x5d,1,2,
    17,0x21,0,1,1,0x21,XINPUT_EP_IN,20,0,0,0,0,0x11,XINPUT_EP_OUT,8,0,0,
    7,5,XINPUT_EP_IN,3,32,0,2,
    7,5,XINPUT_EP_OUT,3,32,0,8,
    9,4,1,0,1,3,0,0,0, /* Native HID status interface */
    9,0x21,0x11,0x01,0,1,0x22,STATUS_HID_DESCRIPTOR_SIZE,0,
    7,5,0x81,3,32,0,100,
};
const uint8_t xinput_compatible_id[40] = {
    40,0,0,0,0,1,4,0,1,0,0,0,0,0,0,0,
    0,1,'X','U','S','B','1','0',0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0
};
static const uint8_t ms_string[18] __attribute__((aligned(2))) = {
    18,3,'M',0,'S',0,'F',0,'T',0,'1',0,'0',0,'0',0,XINPUT_VENDOR_CODE,0
};
static const tusb_desc_device_t device = {
    .bLength = sizeof(tusb_desc_device_t), .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200, .bMaxPacketSize0 = 64, .idVendor = 0x2e8a,
    .idProduct = 0x10c8, .bcdDevice = 0x0500, .iManufacturer = 1,
    .iProduct = 2, .iSerialNumber = 3, .bNumConfigurations = 1,
};
uint8_t const *tud_descriptor_device_cb(void) { return (const uint8_t *)&device; }
uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    return index == 0 ? xinput_configuration : NULL;
}
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    static uint16_t text[64];
    static char serial[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];
    const char *s;
    if (index == 0xee) return (const uint16_t *)ms_string;
    if (index == 0) { text[0] = 0x0304; text[1] = 0x0409; return text; }
    if (index == 1) s = "Local Receiver Project";
    else if (index == 2) s = "Pico 2 W XInput R5";
    else if (index == 3) {
        if (!serial[0]) pico_get_unique_board_id_string(serial, sizeof(serial));
        s = serial;
    } else return NULL;
    unsigned n = (unsigned)strlen(s); if (n > 63) n = 63;
    text[0] = (uint16_t)(0x0300 | (n * 2 + 2));
    for (unsigned i = 0; i < n; ++i) text[i + 1] = (uint8_t)s[i];
    return text;
}

/* Vendor usage page FF00, usage 1. One 31-byte feature payload plus Report ID.
 * A small input item makes a conventional HID collection; no periodic reports. */
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    static const uint8_t report[] = {
        0x06,0x00,0xff,0x09,1,0xa1,1,0x85,1,0x15,0,0x26,0xff,0,
        0x75,8,0x95,31,0x09,1,0xb1,2,
        /* Local Usage is consumed by Feature; Input needs its own Usage. */
        0x09,1,0x81,2,
        0x85,2,0x09,2,0xb1,2,
        0x85,3,0x09,3,0xb1,2,
        0xc0
    };
    _Static_assert(sizeof(report) == STATUS_HID_DESCRIPTOR_SIZE, "HID descriptor length mismatch");
    return instance ? NULL : report;
}
