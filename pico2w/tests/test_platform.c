// SPDX-License-Identifier: Apache-2.0
/* Execute the real application callbacks with the pinned Bluepad32/BTstack
 * headers. Radio mocks model asynchronous inquiry cancellation and scan intent
 * diverging from the physical radio. No controller or USB hardware is accessed. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pico/time.h"
#include "uni.h"
#include "platform.h"
#include "bridge.h"
#include "usb_receiver.h"
#include "ble_discovery.h"
#include "battery.h"
void receiver_battery_init(void) {}
void receiver_battery_attach(uni_hid_device_t *d, pad_kind_t k, uint32_t now) { (void)d; (void)k; (void)now; }
void receiver_battery_detach(void) {}
void receiver_battery_service(uint32_t now) { (void)now; }

static uint32_t clock_ms;
static btstack_timer_source_t *service_timer;
static bridge_t *usb_bridge;
static uni_hid_device_t device;
static bool scan_intent, ble_scan, classic_scan, incoming, pressed;
static bool pending_le_connect;
static unsigned connect_cancels, scan_stops, inquiry_stops;
static bool le_enabled = true, allowlist;
static HCI_STATE radio_state = HCI_STATE_WORKING;
static gap_connection_type_t link_type = GAP_CONNECTION_INVALID;
static uint32_t cancel_until;
static unsigned scan_calls, deletes, key_deletes, reboots, checks;
static unsigned bond_refreshes, bond_clears, scan_type;
static unsigned accepts, declines, supervision;
static struct uni_platform *platform;
static config_protocol_t config_proto;
static unsigned config_saves;
static bool save_failure, block_power_off;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)
absolute_time_t get_absolute_time(void) { return clock_ms; }
uint32_t to_ms_since_boot(absolute_time_t time) { return (uint32_t)time; }
void cyw43_arch_gpio_put(unsigned pin, bool value) { (void)pin; (void)value; }
bool receiver_bootsel_pressed(void) { return pressed; }
void watchdog_reboot(uint32_t pc, uint32_t sp, uint32_t delay) { (void)pc; (void)sp; (void)delay; ++reboots; }
void btstack_run_loop_set_timer_handler(btstack_timer_source_t *ts, void (*handler)(btstack_timer_source_t *)) { ts->process = handler; service_timer = ts; }
void btstack_run_loop_set_timer(btstack_timer_source_t *ts, uint32_t timeout) { (void)ts; assert(timeout == 0); }
void btstack_run_loop_add_timer(btstack_timer_source_t *ts) { (void)ts; }
void receiver_usb_init(bridge_t *b) { usb_bridge = b; bridge_usb_active(b, true); }
config_protocol_t *receiver_usb_config(void) { return &config_proto; }
bool config_flash_load(receiver_config_t *c,uint32_t *generation) { config_defaults(c);*generation=0;return false; }
bool config_flash_save(const receiver_config_t *c,uint32_t *generation) { assert(radio_state==HCI_STATE_OFF && config_valid(c));config_saves++;if(save_failure)return false;(*generation)++;return true; }
void receiver_usb_service(uint32_t now) { (void)now; }
void uni_gamepad_set_mappings_type(uni_gamepad_mappings_type_t type) { (void)type; }
bool uni_bt_le_is_enabled(void) { return le_enabled; }
void uni_bt_le_set_enabled(bool enable) { le_enabled = enable; }
bool uni_bt_allowlist_is_enabled(void) { return allowlist; }
void uni_bt_allowlist_set_enabled(bool enable) { allowlist = enable; }
bool uni_bt_allowlist_remove_all(void) { allowlist = false; return true; }
void uni_bt_allow_incoming_connections(bool allow) { incoming = allow; }
bool uni_bt_incoming_connections_is_allowed(void) { return incoming; }
bool uni_bt_allowlist_is_allowed_addr(bd_addr_t addr) { (void)addr; return true; }
void gap_set_link_supervision_timeout(uint16_t timeout) { supervision=timeout; }
void l2cap_accept_connection(uint16_t channel) { (void)channel; ++accepts; }
void l2cap_decline_connection(uint16_t channel) { (void)channel; ++declines; }
uni_hid_device_t *uni_hid_device_get_instance_for_address(bd_addr_t addr) {
    return memcmp(addr,device.conn.btaddr,6) ? NULL : &device;
}
uni_hid_device_t *uni_hid_device_create(bd_addr_t addr) {
    static const bd_addr_t zero={0};
    if(memcmp(device.conn.btaddr,zero,6)) return NULL;
    memset(&device,0,sizeof(device)); memcpy(device.conn.btaddr,addr,6);
    device.conn.handle=UNI_BT_CONN_HANDLE_INVALID;
    device.conn.state=UNI_BT_CONN_STATE_DEVICE_DISCOVERED; return &device;
}
void uni_hid_device_set_connection_handle(uni_hid_device_t *d, hci_con_handle_t handle) { d->conn.handle=handle; }
void uni_hid_device_set_incoming(uni_hid_device_t *d, bool value) { d->conn.incoming=value; }
void receiver_ble_refresh_bonds(void) { ++bond_refreshes; }
void receiver_ble_clear_cache(void) { ++bond_clears; }
void gap_set_scan_parameters(uint8_t type, uint16_t interval, uint16_t window) {
    assert(interval == 48 && window == 48); scan_type = type;
}
void gap_start_scan(void) { ble_scan = !pending_le_connect; ++scan_calls; }
void gap_stop_scan(void) { ble_scan = false; ++scan_stops; }
int gap_inquiry_stop(void) { classic_scan = false; cancel_until = clock_ms + 500; ++inquiry_stops; return 0; }
uint8_t gap_connect_cancel(void) { pending_le_connect = false; ++connect_cancels; return ERROR_CODE_SUCCESS; }
uint8_t gap_inquiry_periodic_start(uint8_t length, uint16_t max, uint16_t min) {
    (void)length; (void)max; (void)min;
    if (classic_scan || (int32_t)(clock_ms - cancel_until) < 0) return ERROR_CODE_COMMAND_DISALLOWED;
    classic_scan = true; return ERROR_CODE_SUCCESS;
}
void uni_bt_start_scanning_and_autoconnect_unsafe(void) {
    /* Same intent guard as Bluepad32 update_scanning_state(). */
    if (scan_intent) return;
    scan_intent = true; gap_start_scan(); (void)gap_inquiry_periodic_start(3, 5, 4);
}
void uni_bt_stop_scanning_unsafe(void) {
    scan_intent = false; ble_scan = classic_scan = false;
    cancel_until = clock_ms + 500;
}
int uni_bt_get_gap_inquiry_length(void) { return 3; }
int uni_bt_get_gap_max_periodic_length(void) { return 5; }
int uni_bt_get_gap_min_periodic_length(void) { return 4; }
HCI_STATE hci_get_state(void) { return radio_state; }
int hci_power_control(HCI_POWER_MODE mode) { if (mode == HCI_POWER_OFF && !block_power_off) radio_state = HCI_STATE_OFF; return 0; }
gap_connection_type_t gap_get_connection_type(hci_con_handle_t handle) { (void)handle; return link_type; }
uni_hid_device_t *uni_hid_device_get_instance_for_idx(int32_t idx) { return idx == 0 ? &device : NULL; }
bool uni_hid_device_is_virtual_device(const uni_hid_device_t *d) { return d->parent != NULL; }
#ifndef TEST_VENDOR_DISCONNECT
void uni_hid_device_disconnect(uni_hid_device_t *d) { d->conn.connected = false; platform->on_device_disconnected(d); }
#endif
void uni_hid_device_delete(uni_hid_device_t *d) { memset(d, 0, sizeof(*d)); d->conn.handle = UNI_BT_CONN_HANDLE_INVALID; ++deletes; }
void uni_bt_del_keys_unsafe(void) { ++key_deletes; }
static void advance(uint32_t duration) {
    for (uint32_t i = 0; i < duration; ++i) { ++clock_ms; service_timer->process(service_timer); }
}
static void prepare(pad_kind_t kind) {
    memset(&device, 0, sizeof(device)); device.conn.btaddr[0] = 1;
    device.conn.connected = true; device.conn.handle = 7;
    device.conn.state = UNI_BT_CONN_STATE_DEVICE_PENDING_READY;
    device.connection_timer.timeout = clock_ms + HID_DEVICE_CONNECTION_TIMEOUT_MS;
    if (kind == PAD_SWITCH_PRO) {
        device.vendor_id = 0x057e; device.product_id = 0x2009;
        device.controller_type = CONTROLLER_TYPE_SwitchProController;
        device.conn.protocol = UNI_BT_CONN_PROTOCOL_BR_EDR; link_type = GAP_CONNECTION_ACL;
    } else {
        device.conn.protocol = UNI_BT_CONN_PROTOCOL_BLE; link_type = GAP_CONNECTION_LE;
        device.vendor_id = kind == PAD_XSX ? 0x045e : 0x18d1;
        device.product_id = kind == PAD_XSX ? 0x0b13 : 0x9400;
        device.controller_type = CONTROLLER_TYPE_XBoxOneController;
    }
}
static void remote_disconnect(void) {
    /* Vendor callback occurs before slot teardown; it must not start scanning. */
    device.conn.connected = false;
    platform->on_device_disconnected(&device);
    memset(&device, 0, sizeof(device)); link_type = GAP_CONNECTION_INVALID;
}
void __real_uni_bt_bredr_on_l2cap_incoming_connection(uint16_t, const uint8_t *, uint16_t);
void __wrap_uni_bt_bredr_on_l2cap_incoming_connection(uint16_t, const uint8_t *, uint16_t);
int main(void) {
    platform = receiver_platform(); platform->init(0, NULL); platform->on_init_complete();
    advance(101); CHECK(scan_intent && ble_scan && classic_scan && incoming);
    CHECK(scan_type == 1 && bond_refreshes == 1);
    CHECK(supervision == 8000);
    for (pad_kind_t kind = PAD_XSX; kind <= PAD_STADIA; ++kind) {
        for (int cycle = 0; cycle < 5; ++cycle) {
            prepare(kind); CHECK(platform->on_device_ready(&device) == UNI_ERROR_SUCCESS);
            device.conn.state = UNI_BT_CONN_STATE_DEVICE_READY;
            CHECK(usb_bridge->kind == kind); CHECK(!ble_scan && !incoming);
            uni_controller_t ctl = {.klass = UNI_CONTROLLER_CLASS_GAMEPAD}; ctl.gamepad.buttons = 1;
            platform->on_controller_data(&device, &ctl); CHECK(usb_bridge->report[3] != 0);
            unsigned before = scan_calls; advance(5000);
            CHECK(scan_calls == before); CHECK(usb_bridge->report[3] != 0); /* Silent healthy pad keeps held input. */
            remote_disconnect(); CHECK(usb_bridge->kind == PAD_NONE); CHECK(usb_bridge->report[3] == 0);
            CHECK(scan_calls == before); advance(99); CHECK(scan_calls == before);
            advance(2); CHECK(ble_scan && classic_scan && incoming);
        }
    }
    /* Early power-off while Classic cancellation is pending rejects first start. */
    prepare(PAD_SWITCH_PRO); CHECK(platform->on_device_ready(&device) == UNI_ERROR_SUCCESS);
    device.conn.state = UNI_BT_CONN_STATE_DEVICE_READY; remote_disconnect();
    advance(101); CHECK(ble_scan && !classic_scan && scan_intent);
    advance(2000); CHECK(classic_scan);
    /* Actual BLE scan lost while Bluepad32 intent remains true: start alone no-ops. */
    ble_scan = false; advance(2000); CHECK(ble_scan && incoming);
    /* An idle controller-level scan state is repaired with disable/enable, not
     * just repeated writes of enabled=true. Classic cancellation is async. */
    unsigned stops_before = scan_stops, inquiry_before = inquiry_stops;
    advance(12500);
    CHECK(scan_stops > stops_before && inquiry_stops > inquiry_before);
    CHECK(ble_scan && classic_scan && incoming);
    /* The vendor may time out and delete an unconnected BLE slot without a
     * disconnect callback. Its still-pending LE request must be cancelled. */
    prepare(PAD_STADIA); device.conn.connected = false;
    device.conn.handle = UNI_BT_CONN_HANDLE_INVALID; pending_le_connect = true;
    unsigned cancels_before = connect_cancels;
    unsigned before = scan_calls; advance(6000);
    CHECK(connect_cancels == cancels_before && scan_calls == before);
    uni_hid_device_delete(&device); /* Models vendor's 20 s timeout teardown. */
    advance(2200);
    CHECK(!pending_le_connect && connect_cancels > cancels_before && ble_scan);
    /* Do not repair radio scanning during a new pairing/setup attempt. */
    prepare(PAD_XSX); device.conn.connected = false;
    before = scan_calls; ble_scan = false; advance(6000);
    CHECK(scan_calls == before && !ble_scan);
    platform->on_device_disconnected(&device); memset(&device, 0, sizeof(device));
    advance(101); CHECK(ble_scan);
    /* A lost vendor timeout must not reserve the only slot forever. Exercise
     * all three protocols, and never use input silence as a BLE timeout. */
    for (pad_kind_t kind = PAD_XSX; kind <= PAD_STADIA; ++kind) {
        prepare(kind); device.conn.connected = false;
        device.conn.handle = UNI_BT_CONN_HANDLE_INVALID;
        before = deletes; stops_before = scan_stops; cancels_before = connect_cancels;
        advance(24900);
        CHECK(deletes == before && scan_stops == stops_before && connect_cancels == cancels_before);
        advance(2500);
        CHECK(deletes == before + 1 && usb_bridge->kind == PAD_NONE && ble_scan && incoming);
    }
    /* A fresh attempt to the same address gets its own setup budget. */
    prepare(PAD_XSX); device.conn.connected = false; advance(15000);
    before = deletes;
    prepare(PAD_XSX); device.conn.connected = false; advance(15000);
    CHECK(deletes == before);
    remote_disconnect(); advance(2200);
    /* Missed application disconnect callback, slot has already been cleared. */
    prepare(PAD_STADIA); CHECK(platform->on_device_ready(&device) == UNI_ERROR_SUCCESS);
    device.conn.state = UNI_BT_CONN_STATE_DEVICE_READY;
    memset(&device, 0, sizeof(device)); link_type = GAP_CONNECTION_INVALID;
    advance(2200); CHECK(usb_bridge->kind == PAD_NONE && ble_scan && incoming);
    /* Dead HCI link with an uncleared READY slot must release the device. */
    prepare(PAD_XSX); CHECK(platform->on_device_ready(&device) == UNI_ERROR_SUCCESS);
    device.conn.state = UNI_BT_CONN_STATE_DEVICE_READY; link_type = GAP_CONNECTION_INVALID;
    before = deletes; advance(2200); CHECK(deletes == before + 1);
    CHECK(usb_bridge->kind == PAD_NONE && ble_scan && incoming);
    /* Disconnected READY slot retained after callback must not block scanning. */
    prepare(PAD_SWITCH_PRO); CHECK(platform->on_device_ready(&device)==UNI_ERROR_SUCCESS);
    device.conn.state=UNI_BT_CONN_STATE_DEVICE_READY; device.conn.connected=false;
    platform->on_device_disconnected(&device); before=deletes;
    advance(2200); CHECK(deletes==before+1 && ble_scan && incoming);
    /* Full-mode Switch continuously reports even when button values don't change. */
    prepare(PAD_SWITCH_PRO); CHECK(platform->on_device_ready(&device)==UNI_ERROR_SUCCESS);
    device.conn.state=UNI_BT_CONN_STATE_DEVICE_READY;
    uni_controller_t held={.klass=UNI_CONTROLLER_CLASS_GAMEPAD}; held.gamepad.buttons=1;
    platform->on_controller_data(&device,&held);
    uint8_t full[12]={0x30};
    for(int i=0;i<800;i++) { receiver_switch_report_received(&device,full,12); advance(15); }
    CHECK(usb_bridge->kind==PAD_SWITCH_PRO && usb_bridge->report[3]!=0);
    /* Malformed traffic doesn't keep a sleeping Switch's continuous stream alive. */
    for(int i=0;i<61;i++) { receiver_switch_report_received(&device,full,2); advance(100); }
    CHECK(usb_bridge->kind==PAD_NONE && usb_bridge->report[3]==0);
    advance(2200); CHECK(ble_scan && incoming);
    /* A legitimately event-driven Switch remains connected while silent. */
    prepare(PAD_SWITCH_PRO); CHECK(platform->on_device_ready(&device)==UNI_ERROR_SUCCESS);
    device.conn.state=UNI_BT_CONN_STATE_DEVICE_READY;
    receiver_switch_report_received(&device,full,12);
    uint8_t event_mode[12]={0x3f}; receiver_switch_report_received(&device,event_mode,12);
    advance(12000); CHECK(usb_bridge->kind==PAD_SWITCH_PRO);
    remote_disconnect(); advance(2200);
    /* Execute the pinned vendor incoming handler: the old incoming=false gate
     * rejects the same controller before its READY-slot recovery can run. */
    uint8_t wake[16]={L2CAP_EVENT_INCOMING_CONNECTION,14,0,0,0,0,0,1,7,0,0x11,0,0x40,0,0x50,0};
    unsigned old_accepts, old_declines;
    for (int wake_cycle=0; wake_cycle<5; ++wake_cycle) {
    prepare(PAD_SWITCH_PRO); CHECK(platform->on_device_ready(&device)==UNI_ERROR_SUCCESS);
    device.conn.state=UNI_BT_CONN_STATE_DEVICE_READY;
    wake[10]=0x11; wake[12]=0x40;
    old_accepts=accepts; old_declines=declines; before=deletes;
    __real_uni_bt_bredr_on_l2cap_incoming_connection(0x40,wake,sizeof(wake));
    CHECK(accepts==old_accepts && declines==old_declines+1 && deletes==before);
    wake[7]=2; __wrap_uni_bt_bredr_on_l2cap_incoming_connection(0x40,wake,sizeof(wake));
    CHECK(accepts==old_accepts && declines==old_declines+2 && !incoming);
    wake[7]=1; wake[10]=0x13;
    __wrap_uni_bt_bredr_on_l2cap_incoming_connection(0x40,wake,sizeof(wake));
    CHECK(accepts==old_accepts && deletes==before && !incoming);
    wake[10]=0x11;
    __wrap_uni_bt_bredr_on_l2cap_incoming_connection(0x40,wake,8);
    CHECK(deletes==before && !incoming);
    __wrap_uni_bt_bredr_on_l2cap_incoming_connection(0x40,wake,sizeof(wake));
    CHECK(deletes==before+1 && accepts==old_accepts+1 && incoming);
    CHECK(usb_bridge->kind==PAD_NONE && device.conn.control_cid==0x40 && device.conn.incoming);
    /* The interrupt request follows immediately, before the 100ms scan timer. */
    wake[10]=0x13; wake[12]=0x41;
    __wrap_uni_bt_bredr_on_l2cap_incoming_connection(0x41,wake,sizeof(wake));
    CHECK(accepts==old_accepts+2 && device.conn.interrupt_cid==0x41);
    device.vendor_id=0x057e; device.product_id=0x2009;
    device.controller_type=CONTROLLER_TYPE_SwitchProController;
    device.conn.connected=true; device.conn.protocol=UNI_BT_CONN_PROTOCOL_BR_EDR;
    device.conn.state=UNI_BT_CONN_STATE_DEVICE_PENDING_READY;
    CHECK(platform->on_device_ready(&device)==UNI_ERROR_SUCCESS);
    device.conn.state=UNI_BT_CONN_STATE_DEVICE_READY;
    CHECK(usb_bridge->kind==PAD_SWITCH_PRO && !incoming);
    remote_disconnect(); advance(2200);
    }
    /* Active BLE controller keeps the original incoming policy. */
    prepare(PAD_XSX); CHECK(platform->on_device_ready(&device)==UNI_ERROR_SUCCESS);
    device.conn.state=UNI_BT_CONN_STATE_DEVICE_READY; wake[10]=0x11;
    advance(12000); CHECK(usb_bridge->kind==PAD_XSX);
    old_accepts=accepts; before=deletes;
    __wrap_uni_bt_bredr_on_l2cap_incoming_connection(0x40,wake,sizeof(wake));
    CHECK(accepts==old_accepts && deletes==before && usb_bridge->kind==PAD_XSX && !incoming);
    remote_disconnect(); advance(2200); CHECK(key_deletes==0);
    /* Short BOOTSEL repairs actual scanning without deleting bonds. */
    ble_scan = false; pressed = true; advance(80); pressed = false; advance(140);
    CHECK(ble_scan && key_deletes == 0);
    /* Long BOOTSEL suppresses pending reconnection and waits for release. */
    pressed = true; advance(3100); CHECK(!incoming && !ble_scan && key_deletes == 1 && reboots == 0);
    before = scan_calls; advance(3000); CHECK(scan_calls == before && reboots == 0);
    pressed = false; advance(120); CHECK(reboots > 0 && scan_calls == before);
    CHECK(bond_clears == 1);
    printf("Platform reconnect: %u checks passed (15 reconnect cycles + 5 Switch wake cycles).\n", checks);
    return 0;
}
