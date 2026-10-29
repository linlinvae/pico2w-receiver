// SPDX-License-Identifier: Apache-2.0
/* Use the pinned vendor disconnect and connection-timeout implementations.
 * HCI initiation and radio scanning remain models, not real radio evidence. */
#define TEST_VENDOR_DISCONNECT
#define main legacy_platform_test_main
#include "test_platform.c"
#undef main

void receiver_test_vendor_timeout(uni_hid_device_t *d);
static unsigned disconnect_callbacks;
void uni_bt_conn_disconnect(uni_bt_conn_t *c) { c->connected = false; }
void uni_bt_le_disconnect(uni_hid_device_t *d) { (void)d; }
void uni_bt_bredr_disconnect(uni_hid_device_t *d) { (void)d; }
void uni_hid_device_dump_device(uni_hid_device_t *d) { (void)d; }
void uni_hid_device_on_connected(uni_hid_device_t *d, bool value) {
    if (!value) { ++disconnect_callbacks; platform->on_device_disconnected(d); }
}
int btstack_run_loop_remove_timer(btstack_timer_source_t *ts) { (void)ts; return 0; }
void *btstack_run_loop_get_timer_context(btstack_timer_source_t *ts) { return ts->context; }

int main(void) {
    platform = receiver_platform(); platform->init(0, NULL); platform->on_init_complete(); advance(101);
    for (pad_kind_t kind = PAD_XSX; kind <= PAD_STADIA; kind += 2) {
        prepare(kind); device.conn.connected = false;
        device.conn.handle = UNI_BT_CONN_HANDLE_INVALID;
        device.conn.state = UNI_BT_CONN_STATE_DEVICE_DISCOVERED;
        link_type = GAP_CONNECTION_INVALID;
        pending_le_connect = true; ble_scan = false;
        unsigned before = connect_cancels, old_deletes = deletes;
        advance(HID_DEVICE_CONNECTION_TIMEOUT_MS);
        CHECK(connect_cancels == before && deletes == old_deletes);
        receiver_test_vendor_timeout(&device);
        /* Actual vendor code has neither a valid HCI handle nor connected=true:
         * it deletes the slot without notifying the application or cancelling
         * the outgoing LE request. Repeated scan-start alone cannot fix it. */
        CHECK(deletes == old_deletes + 1 && disconnect_callbacks == 0 && pending_le_connect);
        advance(2200);
        CHECK(!pending_le_connect && connect_cancels > before && ble_scan && incoming);
        prepare(kind); CHECK(platform->on_device_ready(&device) == UNI_ERROR_SUCCESS);
        device.conn.state = UNI_BT_CONN_STATE_DEVICE_READY;
        before = connect_cancels; unsigned old_stops = scan_stops;
        advance(60000);
        CHECK(usb_bridge->kind == kind && connect_cancels == before && scan_stops == old_stops);
        remote_disconnect(); advance(2200);
    }
    CHECK(!key_deletes && !reboots && !config_saves);
    printf("Vendor setup-timeout recovery: %u checks passed (Xbox/Stadia, no radio hardware).\n", checks);
    return 0;
}
