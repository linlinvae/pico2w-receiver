// SPDX-License-Identifier: Apache-2.0
#include "usb_xinput.h"
#include "usb_descriptors.h"
#include "tusb.h"
#include "device/usbd_pvt.h"
#include "pico/unique_id.h"
#include <string.h>
/* Feature payload is independent of the Bluetooth implementation. */
extern void receiver_battery_status(uint8_t out[31]);
static bridge_t *bridge;
static config_protocol_t config;
config_protocol_t *usb_xinput_config(void) { return &config; }
static bool opened;
static uint32_t last_report;
static uint8_t rx[32] __attribute__((aligned(4)));
static uint8_t tx[20] __attribute__((aligned(4)));
static uint8_t control_buffer[20] __attribute__((aligned(4)));
static void reset(uint8_t port) {
    (void)port; opened = false;
    if (bridge) bridge_usb_active(bridge, false);
}
static void init(void) { reset(0); }
static uint16_t open_driver(uint8_t port, const tusb_desc_interface_t *intf, uint16_t max_len) {
    if (max_len < 40 || intf->bInterfaceNumber || intf->bAlternateSetting ||
        intf->bInterfaceClass != 0xff || intf->bInterfaceSubClass != 0x5d ||
        intf->bInterfaceProtocol != 1 || intf->bNumEndpoints != 2) return 0;
    const uint8_t *p = (const uint8_t *)intf;
    if (p[9] != 17 || p[10] != 0x21) return 0;
    const tusb_desc_endpoint_t *in = (const tusb_desc_endpoint_t *)(p + 26);
    const tusb_desc_endpoint_t *out = (const tusb_desc_endpoint_t *)(p + 33);
    if (in->bLength != 7 || out->bLength != 7 || in->bDescriptorType != TUSB_DESC_ENDPOINT ||
        out->bDescriptorType != TUSB_DESC_ENDPOINT || in->bEndpointAddress != XINPUT_EP_IN ||
        out->bEndpointAddress != XINPUT_EP_OUT) return 0;
    if (!usbd_edpt_open(port, in) || !usbd_edpt_open(port, out)) return 0;
    opened = true;
    usbd_edpt_xfer(port, XINPUT_EP_OUT, rx, sizeof(rx));
    return 40;
}
static bool control(uint8_t port, uint8_t stage, const tusb_control_request_t *r) {
    if (r->wIndex != 0) return false;
    if (stage == CONTROL_STAGE_SETUP) {
        if (r->bmRequestType == 0xa1 && r->bRequest == 1 && r->wValue == 0x0100) {
            bridge_report(bridge, control_buffer);
            return tud_control_xfer(port, r, control_buffer, 20);
        }
        if (r->bmRequestType == 0x21 && r->bRequest == 9 && r->wValue == 0x0200 && r->wLength == 8) {
            /* Tail bytes must be zero in XUSB. A sentinel rejects short EP0 OUT
             * packets because TinyUSB's ACK callback does not expose their length. */
            memset(control_buffer, 0xff, sizeof(control_buffer));
            return tud_control_xfer(port, r, control_buffer, 8);
        }
        if (r->bmRequestType == 0x81 && r->bRequest == TUSB_REQ_GET_DESCRIPTOR && r->wValue == 0x2100)
            return tud_control_xfer(port, r, (void *)(xinput_configuration + 18), 17);
        return false;
    }
    if (stage == CONTROL_STAGE_ACK && r->bmRequestType == 0x21 && r->bRequest == 9 &&
        r->wValue == 0x0200 && r->wLength == 8)
        bridge_output(bridge, control_buffer, 8);
    return true;
}
bool tud_vendor_control_xfer_cb(uint8_t port, uint8_t stage, const tusb_control_request_t *r) {
    if (stage != CONTROL_STAGE_SETUP) return true;
    if (r->bmRequestType == 0xc0 && r->bRequest == XINPUT_VENDOR_CODE && !r->wValue && r->wIndex == 4)
        return tud_control_xfer(port, r, (void *)xinput_compatible_id, 40);
    if (r->bmRequestType == 0xc0 && r->bRequest == 1 && !r->wValue && !r->wIndex) {
        static uint8_t id[4];
        pico_unique_board_id_t unique;
        pico_get_unique_board_id(&unique);
        /* Fold all eight OTP ID bytes into the wired XUSB four-byte ID. */
        for (unsigned i = 0; i < 4; ++i) id[i] = unique.id[i] ^ unique.id[i + 4];
        id[3] &= 0x0f;
        return tud_control_xfer(port, r, (void *)id, sizeof(id));
    }
    if (r->bmRequestType == 0x41 && r->bRequest == 0 && !r->wIndex && !r->wLength)
        return tud_control_status(port, r);
    return false;
}
static bool transfer(uint8_t port, uint8_t ep, xfer_result_t result, uint32_t count) {
    if (ep == XINPUT_EP_OUT) {
        if (result == XFER_RESULT_SUCCESS) bridge_output(bridge, rx, count);
        usbd_edpt_xfer(port, XINPUT_EP_OUT, rx, sizeof(rx));
    }
    return true;
}
const usbd_class_driver_t *usbd_app_driver_get_cb(uint8_t *count) {
    static const usbd_class_driver_t driver = {
        .name = "XUSB", .init = init, .reset = reset, .open = open_driver,
        .control_xfer_cb = control, .xfer_cb = transfer,
    };
    *count = 1; return &driver;
}
void tud_mount_cb(void) { bridge_usb_active(bridge, true); }
void tud_umount_cb(void) { bridge_usb_active(bridge, false); }
void tud_suspend_cb(bool remote_wakeup_en) {
    (void)remote_wakeup_en; bridge_usb_active(bridge, false);
}
void tud_resume_cb(void) { bridge_usb_active(bridge, true); }
void usb_xinput_init(bridge_t *state) { bridge = state;config_protocol_init(&config,state,0);tusb_init(); }
void usb_xinput_service(uint32_t now) {
    tud_task();
    if (!opened || !tud_ready()) return;
    if ((uint32_t)(now - last_report) < 2 && !bridge->force_neutral) return;
    if (!usbd_edpt_claim(0, XINPUT_EP_IN)) return;
    bridge_report(bridge, tx);
    if (usbd_edpt_xfer(0, XINPUT_EP_IN, tx, sizeof(tx))) {
        last_report = now; bridge_report_sent(bridge);
    } else usbd_edpt_release(0, XINPUT_EP_IN);
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t type, uint8_t *buffer, uint16_t length) {
    if (instance || type != HID_REPORT_TYPE_FEATURE || length < 31) return 0;
    if(report_id==1)receiver_battery_status(buffer);
    else if(report_id==3)memcpy(buffer,config.response,31);
    else return 0;
    return 31;
}
void tud_hid_set_report_cb(uint8_t instance, uint8_t id, hid_report_type_t type, const uint8_t *buffer, uint16_t length) {
    if(!instance && id==2 && type==HID_REPORT_TYPE_FEATURE)config_protocol_set(&config,buffer,length);
}
