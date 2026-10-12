// SPDX-License-Identifier: Apache-2.0
#include <string.h>
#include "pico/cyw43_arch.h"
#include "pico/time.h"
#include "hardware/watchdog.h"
#include "uni.h"
#include "bt/uni_bt_allowlist.h"
#include "bt/uni_bt_le.h"
#include "platform.h"
#include "bridge.h"
#include "usb_receiver.h"
#include "board_button.h"
#include "ble_discovery.h"
#include "battery.h"
static bridge_t bridge;
static uni_hid_device_t *pad;
static btstack_timer_source_t timer;
static bool forgetting, keys_deleted, initialized, was_pressed, long_press;
static uint32_t button_down, button_up, last_button, last_rumble, last_led;
static bool scan_pending;
static uint32_t scan_after, last_scan_repair, last_connection_check;
static bool switch_full_stream;
static uint32_t switch_last_report;
static unsigned save_phase;
static uint32_t save_since;
static uint32_t now_ms(void) { return to_ms_since_boot(get_absolute_time()); }
static void request_scan(void) {
    /* Disconnect callbacks run before Bluepad32 deletes the device and BTstack
     * releases its connection. Resume from the foreground timer after teardown. */
    scan_pending = true; scan_after = now_ms() + 100;
}
static bool device_slot_busy(void) {
    static const bd_addr_t zero_addr = {0};
    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; ++i) {
        uni_hid_device_t *d = uni_hid_device_get_instance_for_idx(i);
        if (d && memcmp(d->conn.btaddr, zero_addr, sizeof(zero_addr))) return true;
    }
    return false;
}
static void release_pad(void) {
    receiver_battery_detach(); pad = NULL; switch_full_stream = false; bridge_disconnect(&bridge);
    if (!forgetting) {
        /* This only changes a host flag; radio scanning still waits for teardown.
         * A waking Switch can send its interrupt-channel request immediately. */
        uni_bt_allow_incoming_connections(true); request_scan();
    }
}
static void drop_pad(void) {
    uni_hid_device_t *stale = pad;
    release_pad();
    uni_hid_device_disconnect(stale); uni_hid_device_delete(stale);
}
void receiver_switch_report_received(uni_hid_device_t *d, const uint8_t *report, uint16_t len) {
    if (d != pad || bridge.kind != PAD_SWITCH_PRO || forgetting || len < 12) return;
    if (report[0] == 0x30) {
        switch_full_stream = true; switch_last_report = now_ms();
    } else if (report[0] == 0x21 && switch_full_stream) {
        switch_last_report = now_ms();
    } else if (report[0] == 0x3f) {
        switch_full_stream = false; /* Event-driven mode is allowed to be silent. */
    }
}
static void connection_service(uint32_t now) {
    if (!initialized || forgetting || receiver_usb_config()->saving || receiver_usb_config()->rebooting || hci_get_state() != HCI_STATE_WORKING) return;
    if ((uint32_t)(now - last_connection_check) >= 100) {
        last_connection_check = now;
        /* Packet silence is meaningful only for Switch's observed full stream.
         * Other controllers and Switch's event-driven mode may stay silent. */
        if (pad && (!pad->conn.connected || pad->conn.state != UNI_BT_CONN_STATE_DEVICE_READY))
            drop_pad();
        if (pad && (gap_get_connection_type(pad->conn.handle) == GAP_CONNECTION_INVALID ||
            (switch_full_stream && (uint32_t)(now - switch_last_report) >= 6000)))
            drop_pad();
        /* A missed callback can leave a disconnected READY slot after pad=NULL. */
        if (!pad) for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; ++i) {
            uni_hid_device_t *d = uni_hid_device_get_instance_for_idx(i);
            if (d && d->conn.state == UNI_BT_CONN_STATE_DEVICE_READY &&
                (!d->conn.connected || gap_get_connection_type(d->conn.handle) == GAP_CONNECTION_INVALID)) {
                uni_hid_device_disconnect(d); uni_hid_device_delete(d);
            }
        }
    }
    if (pad || device_slot_busy()) return; /* Includes pairing/setup in progress. */
    if (scan_pending && (int32_t)(now - scan_after) < 0) return;
    if (!scan_pending && (uint32_t)(now - last_scan_repair) < 2000) return;
    scan_pending = false; last_scan_repair = now;
    uni_bt_allow_incoming_connections(true);
    uni_bt_start_scanning_and_autoconnect_unsafe();
    /* Bluepad32's flag records intent, not radio state: a repeated start is a
     * no-op. Reassert BLE scanning and retry Classic inquiry underneath it.
     * BTstack rejects an already-active inquiry without changing its state;
     * a start rejected while cancellation was pending is retried next time. */
    gap_start_scan();
    (void)gap_inquiry_periodic_start(uni_bt_get_gap_inquiry_length(),
        uni_bt_get_gap_max_periodic_length(), uni_bt_get_gap_min_periodic_length());
}
static void rumble(uint8_t strong, uint8_t weak) {
    if (pad && pad->report_parser.play_dual_rumble)
        pad->report_parser.play_dual_rumble(pad, 0, (strong || weak) ? 250 : 0, weak, strong);
}
static void configuration_service(uint32_t now) {
    config_protocol_t *p=receiver_usb_config();config_protocol_tick(p,now);
    if(p->rebooting)return;
    if(!p->saving) { save_phase=0;return; }
    if(forgetting || (!save_phase && (!initialized || pad || device_slot_busy()))) {
        config_protocol_saved(p,CFG_NEEDS_IDLE,p->generation);save_phase=0;return;
    }
    if(!save_phase) {
        scan_pending=false;uni_bt_allow_incoming_connections(false);uni_bt_stop_scanning_unsafe();
        hci_power_control(HCI_POWER_OFF);save_phase=1;save_since=now;
    }
    bool off=hci_get_state()==HCI_STATE_OFF;
    if(off || (uint32_t)(now-save_since)>2000) {
        uint32_t generation=p->generation;
        bool ok=off && config_flash_save(&bridge.config,&generation);
        config_protocol_saved(p,ok?CFG_OK:CFG_IO,generation);p->rebooting=true;
        /* Bluepad32's setup index cannot safely restart HCI in-place. Give the
         * host time to read the result, then reinitialize through a full reboot. */
        watchdog_reboot(0,0,750);
    }
}
static void forget(void) {
    forgetting = true; rumble(0, 0); receiver_battery_detach(); bridge_disconnect(&bridge);
    receiver_ble_clear_cache();
    scan_pending = false;
    uni_bt_allow_incoming_connections(false);
    uni_bt_stop_scanning_unsafe();
    /* Radio OFF prevents a late pairing event from recreating deleted bonds. */
    hci_power_control(HCI_POWER_OFF);
}
static void service(btstack_timer_source_t *ts) {
    uint32_t now = now_ms();
    connection_service(now);
    receiver_battery_service(now);
    bridge_tick(&bridge,now);
    receiver_usb_service(now);
    configuration_service(now);
    uint8_t strong, weak;
    if (bridge_take_rumble(&bridge, (uint32_t)(now - last_rumble) >= 100, &strong, &weak)) {
        rumble(strong, weak); last_rumble = now;
    }
    if (initialized && (uint32_t)(now - last_button) >= 20) {
        last_button = now;
        bool pressed = receiver_bootsel_pressed();
        if (pressed && !was_pressed && !forgetting) { button_down = now; long_press = false; }
        if (pressed && !long_press && !forgetting && (uint32_t)(now - button_down) >= 3000) {
            long_press = true; forget();
        }
        if (!pressed && was_pressed) button_up = now;
        if (!pressed && was_pressed && !long_press && !forgetting && (uint32_t)(now - button_down) >= 40 && !pad)
            request_scan();
        was_pressed = pressed;
    }
    if ((uint32_t)(now - last_led) >= 100) {
        last_led = now;
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, forgetting ? ((now / 100) & 1) :
            (pad ? 1 : ((now / 500) & 1)));
    }
    if (forgetting && !keys_deleted && hci_get_state() == HCI_STATE_OFF) {
        uni_bt_del_keys_unsafe();
        uni_bt_allowlist_remove_all(); uni_bt_allowlist_set_enabled(false);
        keys_deleted = true;
    }
    /* Reboot only after release, otherwise ROM may enter BOOTSEL mass storage. */
    if (keys_deleted && !was_pressed && (uint32_t)(now - button_up) >= 100) {
        watchdog_reboot(0, 0, 20); return;
    }
    /* SDK polling backend adds 1 ms to timeout: 0 requests a 1 ms service cadence. */
    btstack_run_loop_set_timer(ts, 0); btstack_run_loop_add_timer(ts);
}
static void init(int argc, const char **argv) {
    (void)argc; (void)argv;
    bridge_init(&bridge);uint32_t generation=0;config_flash_load(&bridge.config,&generation);
    receiver_usb_init(&bridge);config_protocol_init(receiver_usb_config(),&bridge,generation);
    btstack_run_loop_set_timer_handler(&timer, service);
    btstack_run_loop_set_timer(&timer, 0); btstack_run_loop_add_timer(&timer);
    uni_gamepad_set_mappings_type(UNI_GAMEPAD_MAPPINGS_TYPE_XBOX);
    if (!uni_bt_le_is_enabled()) uni_bt_le_set_enabled(true);
}
static void init_complete(void) {
    initialized = true;
    receiver_battery_init();
    gap_set_link_supervision_timeout(8000); /* 8000 * 0.625 ms = 5 s for Classic master links. */
    receiver_ble_refresh_bonds();
    /* Stadia may put identifying fields in the scan response, not ADV_IND. */
    gap_set_scan_parameters(1 /* active */, 48 /* 30 ms */, 48);
    /* Keep multiple controllers' bonds; one active controller at a time. */
    if (uni_bt_allowlist_is_enabled()) uni_bt_allowlist_set_enabled(false);
    request_scan();
}
static uni_error_t discovered(bd_addr_t addr, const char *name, uint16_t cod, uint8_t rssi) {
    (void)addr; (void)cod; (void)rssi;
    if (forgetting || pad || receiver_usb_config()->saving || receiver_usb_config()->rebooting) return UNI_ERROR_IGNORE_DEVICE;
    /* BLE shortened names are legal; strict VID/PID/type validation happens at ready. */
    if (name && name[0] && strcmp(name, "Pro Controller") &&
        !strstr(name, "Xbox") && !strstr(name, "Stadia")) return UNI_ERROR_IGNORE_DEVICE;
    return UNI_ERROR_SUCCESS;
}
static void connected(uni_hid_device_t *d) { (void)d; }
static uni_error_t ready(uni_hid_device_t *d) {
    if (forgetting || pad || receiver_usb_config()->saving || receiver_usb_config()->rebooting || uni_hid_device_is_virtual_device(d)) return UNI_ERROR_INVALID_CONTROLLER;
    pad_kind_t kind = bridge_identify(d->vendor_id, d->product_id,
        d->conn.protocol == UNI_BT_CONN_PROTOCOL_BLE,
        d->controller_type == CONTROLLER_TYPE_SwitchProController,
        d->controller_type == CONTROLLER_TYPE_XBoxOneController);
    if (kind == PAD_NONE) return UNI_ERROR_INVALID_CONTROLLER;
    if (d->conn.protocol == UNI_BT_CONN_PROTOCOL_BLE) receiver_ble_refresh_bonds();
    pad = d; bridge_connect(&bridge, kind);
    switch_full_stream = false;
    receiver_battery_attach(d, kind, now_ms());
    scan_pending = false;
    uni_bt_stop_scanning_unsafe();
    uni_bt_allow_incoming_connections(false);
    if (d->report_parser.set_player_leds) d->report_parser.set_player_leds(d, 1);
    return UNI_ERROR_SUCCESS;
}
static void disconnected(uni_hid_device_t *d) {
    if (d == pad) release_pad();
    else if (!pad && !forgetting && !uni_hid_device_is_virtual_device(d)) request_scan();
}
static void data(uni_hid_device_t *d, uni_controller_t *ctl) {
    if (d != pad || forgetting || ctl->klass != UNI_CONTROLLER_CLASS_GAMEPAD) return;
    const uni_gamepad_t *g = &ctl->gamepad;
    const pad_input_t input = {.buttons = g->buttons, .dpad = g->dpad, .misc = g->misc_buttons,
        .axes = {g->axis_x, g->axis_y, g->axis_rx, g->axis_ry}, .brake = g->brake, .throttle = g->throttle};
    bridge_input(&bridge, &input);
}
static void oob(uni_platform_oob_event_t event, void *payload) { (void)event; (void)payload; }
static const uni_property_t *property(uni_property_idx_t idx) { (void)idx; return NULL; }
void __real_uni_bt_bredr_on_l2cap_incoming_connection(uint16_t, const uint8_t *, uint16_t);
void __wrap_uni_bt_bredr_on_l2cap_incoming_connection(uint16_t channel, const uint8_t *packet, uint16_t size) {
    if (size < 16 || packet[0] != L2CAP_EVENT_INCOMING_CONNECTION || packet[1] < 14 || packet[1] + 2u > size) return;
    bool waking_switch = false;
    if (!forgetting && pad && bridge.kind == PAD_SWITCH_PRO &&
        l2cap_event_incoming_connection_get_psm(packet) == PSM_HID_CONTROL) {
        bd_addr_t address;
        l2cap_event_incoming_connection_get_address(packet, address);
        waking_switch = memcmp(address, pad->conn.btaddr, sizeof(address)) == 0;
    }
    /* Bluepad32 already replaces a READY ghost connection, but the application's
     * incoming=false gate prevented that branch from running. Scope the exception
     * to the same Switch's HID control channel; leave the vendor lifecycle intact. */
    if (waking_switch) uni_bt_allow_incoming_connections(true);
    __real_uni_bt_bredr_on_l2cap_incoming_connection(channel, packet, size);
    if (pad) uni_bt_allow_incoming_connections(false);
}
struct uni_platform *receiver_platform(void) {
    static struct uni_platform p = {.name = RECEIVER_PLATFORM_NAME, .init = init,
        .on_init_complete = init_complete, .on_device_discovered = discovered,
        .on_device_connected = connected, .on_device_disconnected = disconnected,
        .on_device_ready = ready, .on_controller_data = data,
        .on_oob_event = oob, .get_property = property};
    return &p;
}
