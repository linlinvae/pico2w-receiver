// SPDX-License-Identifier: Apache-2.0
#include "usb_switch.h"
#include "switch_report.h"
#include "tusb.h"
#include "device/usbd_pvt.h"
#include "class/hid/hid_device.h"
#include <string.h>
static bridge_t *bridge;
static config_protocol_t config;
static uint32_t last_report;
config_protocol_t *usb_switch_config(void) { return &config; }
/* App drivers run before built-in drivers. Delegate this one HID interface to
 * TinyUSB, adding a bridge reset so a USB bus reset always stops old outputs.
 * TinyUSB also resets its built-in HID driver; the repeated reset is harmless. */
static void reset(uint8_t port) {
    hidd_reset(port); last_report = 0;
    if (bridge) bridge_usb_active(bridge, false);
}
static void init(void) { reset(0); }
static bool control(uint8_t port, uint8_t stage, const tusb_control_request_t *r) {
    if (r->wIndex != 0) return false;
    /* This profile has no Report IDs or Feature reports. TinyUSB would otherwise
     * ACK unknown SET_REPORTs and return an ID-only GET_REPORT for unknown IDs. */
    if (r->bmRequestType == 0xa1 && r->bRequest == HID_REQ_CONTROL_GET_REPORT && r->wValue != 0x0100) return false;
    if (r->bmRequestType == 0x21 && r->bRequest == HID_REQ_CONTROL_SET_REPORT &&
        (r->wValue != 0x0200 || r->wLength != 8)) return false;
    return hidd_control_xfer_cb(port, stage, r);
}
const usbd_class_driver_t *usbd_app_driver_get_cb(uint8_t *count) {
    static const usbd_class_driver_t driver = {
        .name = "Switch HID", .init = init, .reset = reset, .open = hidd_open,
        .control_xfer_cb = control, .xfer_cb = hidd_xfer_cb,
    };
    *count = 1; return &driver;
}
void tud_mount_cb(void) { if (bridge) bridge_usb_active(bridge, true); }
void tud_umount_cb(void) { if (bridge) bridge_usb_active(bridge, false); }
void tud_suspend_cb(bool remote_wakeup_en) {
    (void)remote_wakeup_en;
    if (bridge) bridge_usb_active(bridge, false);
}
void tud_resume_cb(void) { if (bridge) bridge_usb_active(bridge, true); }
void usb_switch_init(bridge_t *state) {
    bridge = state; last_report = 0; config_protocol_init(&config, state, 0); tusb_init();
}
void usb_switch_service(uint32_t now) {
    tud_task();
    if (!bridge || !bridge->usb_active || !tud_ready() || !tud_hid_n_ready(0)) return;
    if ((uint32_t)(now - last_report) < SWITCH_USB_INTERVAL_MS && !bridge->force_neutral) return;
    uint8_t report[SWITCH_REPORT_SIZE]; switch_report(bridge, report);
    /* TinyUSB copies to its endpoint buffer; Bluetooth cannot alter an in-flight
     * report. Never consume a queued release when transfer submission fails. */
    if (tud_hid_n_report(0, 0, report, sizeof(report))) {
        last_report = now; bridge_report_sent(bridge);
    }
}
void tud_hid_report_failed_cb(uint8_t instance, hid_report_type_t type,
        const uint8_t *report, uint16_t transferred) {
    (void)report; (void)transferred;
    if (!instance && type == HID_REPORT_TYPE_INPUT && bridge) bridge->force_neutral = true;
}
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t id, hid_report_type_t type,
        uint8_t *buffer, uint16_t length) {
    if (instance || id || type != HID_REPORT_TYPE_INPUT || !bridge || !buffer) return 0;
    uint8_t report[SWITCH_REPORT_SIZE]; switch_report(bridge, report);
    uint16_t size = length < sizeof(report) ? length : sizeof(report);
    memcpy(buffer, report, size); return size;
}
void tud_hid_set_report_cb(uint8_t instance, uint8_t id, hid_report_type_t type,
        const uint8_t *buffer, uint16_t length) {
    /* The compatibility descriptor has an eight-byte output report, but this
     * wired profile has no rumble protocol. Never interpret it as XUSB or config. */
    (void)instance; (void)id; (void)type; (void)buffer; (void)length;
}
