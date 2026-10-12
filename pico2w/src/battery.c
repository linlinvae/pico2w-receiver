// SPDX-License-Identifier: Apache-2.0
#include "battery.h"
#include "ble/gatt-service/battery_service_client.h"
#include "hid_usage.h"
#include "platform.h"
#include <string.h>
static uni_hid_device_t *device;
static pad_kind_t kind;
static uint16_t cid;
static uint32_t clock_ms, due, updated;
static uint8_t percent = 255, level = 255, charging = 255, source, reason;
static uint8_t hid_raw, hid_min, hid_max;
static bool attempted, bas_ready, read_pending, valid;
/* Reasons: 0=no controller, 1=waiting, 2=unsupported/error, 3=invalid/stale. */
static void event(uint8_t type, uint16_t channel, uint8_t *packet, uint16_t size) {
    (void)channel;
    if (!device || !cid || type != HCI_EVENT_PACKET || size < 8 ||
        packet[0] != HCI_EVENT_GATTSERVICE_META ||
        little_endian_read_16(packet, 3) != cid) return;
    if (packet[2] == GATTSERVICE_SUBEVENT_BATTERY_SERVICE_CONNECTED) {
        if (packet[5]) {
            /* BTstack frees failed clients after this callback. */
            cid = 0; reason = 2;
        } else { bas_ready = true; read_pending = true; due = clock_ms; }
    } else if (packet[2] == GATTSERVICE_SUBEVENT_BATTERY_SERVICE_LEVEL && !packet[5]) {
        if (packet[6] || packet[7] > 100) { reason = 3; return; }
        percent = packet[7]; level = charging = 255; source = 1;
        valid = true; reason = 0; updated = clock_ms;
    }
}
void receiver_battery_init(void) { battery_service_client_init(); }
void receiver_battery_detach(void) {
    uint16_t previous = cid;
    device = NULL; cid = 0; kind = PAD_NONE;
    attempted = bas_ready = read_pending = valid = false;
    percent = level = charging = 255; source = reason = 0;
    hid_raw = hid_min = hid_max = 0;
    if (previous) (void)battery_service_client_disconnect(previous);
}
void receiver_battery_attach(uni_hid_device_t *d, pad_kind_t k, uint32_t now) {
    receiver_battery_detach(); device = d; kind = k; clock_ms = now;
    reason = 1; due = now + 1000; /* Finish the controller's HID setup first. */
}
void receiver_battery_service(uint32_t now) {
    clock_ms = now;
    if (!device) return;
    if (valid && (uint32_t)(now - updated) > 120000) { valid = false; reason = 3; }
    if (kind == PAD_SWITCH_PRO || (int32_t)(now - due) < 0) return;
    if (!attempted) {
        attempted = true;
        uint8_t result = battery_service_client_connect(device->conn.handle, event, 0, &cid);
        if (result) {
            /* Some immediate errors leave an allocated idle client. Release it. */
            if (cid) (void)battery_service_client_disconnect(cid);
            cid = 0; reason = 2;
        }
    } else if (bas_ready && cid) {
        uint8_t result = battery_service_client_read_battery_level(cid, 0);
        read_pending = result != ERROR_CODE_SUCCESS;
        due = now + (read_pending ? 1000 : 30000);
    }
}
void receiver_battery_status(uint8_t out[RECEIVER_STATUS_SIZE]) {
    memset(out, 0, RECEIVER_STATUS_SIZE); memcpy(out, "P2W", 3);
    out[3] = 1; out[4] = (uint8_t)kind; out[5] = valid;
    out[6] = source; out[7] = valid ? percent : 255;
    out[8] = valid ? level : 255; out[9] = valid ? charging : 255;
    out[10] = reason; out[11] = 2;
    uint32_t age = valid ? clock_ms - updated : UINT32_MAX;
    for (unsigned i = 0; i < 4; ++i) out[12 + i] = (uint8_t)(age >> (i * 8));
    out[16] = hid_raw; out[17] = hid_min; out[18] = hid_max;
}
void __real_uni_hid_parser_switch_parse_input_report(uni_hid_device_t *, const uint8_t *, uint16_t);
void __wrap_uni_hid_parser_switch_parse_input_report(uni_hid_device_t *d, const uint8_t *report, uint16_t len) {
    receiver_switch_report_received(d, report, len);
    if (d == device && kind == PAD_SWITCH_PRO && len >= 12 &&
        (report[0] == 0x30 || report[0] == 0x21)) {
        uint8_t value = report[2] >> 5;
        if (value <= 4) {
            level = value; charging = (report[2] >> 4) & 1;
            percent = 255; source = 2; valid = true; reason = 0; updated = clock_ms;
        }
    }
    __real_uni_hid_parser_switch_parse_input_report(d, report, len);
}
void __real_uni_hid_parser_xboxone_parse_usage(uni_hid_device_t *, const hid_globals_t *, uint16_t, uint16_t, int32_t);
void __wrap_uni_hid_parser_xboxone_parse_usage(uni_hid_device_t *d, const hid_globals_t *g,
        uint16_t page, uint16_t usage, int32_t value) {
    if (d == device && kind == PAD_XSX && g && page == HID_USAGE_PAGE_GENERIC_DEVICE_CONTROLS &&
        usage == HID_USAGE_BATTERY_STRENGTH && !(valid && source == 1) && value > 0 &&
        g->logical_minimum >= 0 && g->logical_maximum <= 255 &&
        g->logical_maximum > g->logical_minimum && value >= g->logical_minimum && value <= g->logical_maximum) {
        /* Preserve the descriptor's range; a raw HID value is not a percent. */
        hid_raw = (uint8_t)value; hid_min = (uint8_t)g->logical_minimum; hid_max = (uint8_t)g->logical_maximum;
        source = 3; percent = level = charging = 255; valid = true; reason = 0; updated = clock_ms;
    }
    __real_uni_hid_parser_xboxone_parse_usage(d, g, page, usage, value);
}
