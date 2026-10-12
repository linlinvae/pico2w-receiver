// SPDX-License-Identifier: Apache-2.0
/* Replay advertising packets through the actual pinned Bluepad32 LE discovery
 * function. Only the key DB and radio/device allocation are mocked. */
#include <stdio.h>
#include <string.h>
#include "uni.h"
#include "ble/le_device_db.h"
#include "ble_discovery.h"

static uni_hid_device_t device;
static bool allocated, discovery_allowed = true;
static unsigned creates, connect_calls, checks, db_reads;
static bd_addr_t connected_addr;
static uint8_t connected_type;
static int db_types[MAX_NR_LE_DEVICE_DB_ENTRIES];
static bd_addr_t db_addresses[MAX_NR_LE_DEVICE_DB_ENTRIES];
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)
void __real_uni_bt_le_on_gap_event_advertising_report(const uint8_t *packet, uint16_t size);
int le_device_db_max_count(void) { return MAX_NR_LE_DEVICE_DB_ENTRIES; }
void le_device_db_info(int idx, int *type, bd_addr_t addr, sm_key_t irk) {
    ++db_reads; *type = db_types[idx]; memcpy(addr, db_addresses[idx], sizeof(bd_addr_t)); (void)irk;
}
void uni_property_set(uni_property_idx_t idx, uni_property_value_t value) { (void)idx; (void)value; }
uni_hid_device_t *uni_hid_device_get_instance_for_address(bd_addr_t addr) {
    return allocated && !memcmp(device.conn.btaddr, addr, sizeof(bd_addr_t)) ? &device : NULL;
}
uni_hid_device_t *uni_hid_device_create(bd_addr_t addr) {
    if (allocated) return NULL;
    allocated = true; ++creates; memset(&device, 0, sizeof(device));
    memcpy(device.conn.btaddr, addr, sizeof(bd_addr_t)); return &device;
}
void uni_hid_device_set_cod(uni_hid_device_t *d, uint32_t cod) { d->cod = cod; }
void uni_hid_device_set_name(uni_hid_device_t *d, const char *name) { snprintf(d->name, sizeof(d->name), "%s", name); }
void uni_bt_conn_set_protocol(uni_bt_conn_t *conn, uni_bt_conn_protocol_t protocol) { conn->protocol = protocol; }
void uni_bt_conn_set_state(uni_bt_conn_t *conn, uni_bt_conn_state_t state) { conn->state = state; }
uni_error_t uni_hid_device_on_device_discovered(bd_addr_t addr, const char *name, uint16_t cod, uint8_t rssi) {
    (void)addr; (void)name; (void)cod; (void)rssi;
    return discovery_allowed ? UNI_ERROR_SUCCESS : UNI_ERROR_IGNORE_DEVICE;
}
void gap_stop_scan(void) {}
uint8_t gap_connect(const bd_addr_t addr, bd_addr_type_t type) {
    ++connect_calls; memcpy(connected_addr, addr, sizeof(bd_addr_t)); connected_type = type;
    return ERROR_CODE_SUCCESS;
}
static unsigned advertisement(uint8_t out[43], uint8_t event_type, uint8_t addr_type,
                              const bd_addr_t addr, const uint8_t *ad, unsigned len) {
    memset(out, 0, 43); out[0] = GAP_EVENT_ADVERTISING_REPORT; out[1] = (uint8_t)(10 + len);
    out[2] = event_type; out[3] = addr_type; reverse_bytes(addr, out + 4, 6);
    out[10] = (uint8_t)-52; out[11] = (uint8_t)len;
    if (len) memcpy(out + 12, ad, len);
    return 12 + len;
}
int main(void) {
    const bd_addr_t stadia = {0x10,0x20,0x30,0x40,0x50,0x60};
    const bd_addr_t unknown = {0x10,0x20,0x30,0x40,0x50,0x61};
    uint8_t event[43], original[43];
    for (unsigned i = 0; i < MAX_NR_LE_DEVICE_DB_ENTRIES; ++i) db_types[i] = BD_ADDR_TYPE_UNKNOWN;
    /* Sparse key database: cache must enumerate max entries, not valid count. */
    db_types[7] = BD_ADDR_TYPE_LE_RANDOM; memcpy(db_addresses[7], stadia, sizeof(bd_addr_t));
    receiver_ble_refresh_bonds(); CHECK(db_reads == MAX_NR_LE_DEVICE_DB_ENTRIES);
    unsigned len = advertisement(event, 1 /* directed wake */, 1, stadia, NULL, 0);
    memcpy(original, event, sizeof(event));
    /* R2's original vendor path rejects the same bonded wake-up packet. */
    __real_uni_bt_le_on_gap_event_advertising_report(event, (uint16_t)len);
    CHECK(creates == 0 && connect_calls == 0);
    uni_bt_le_on_gap_event_advertising_report(event, (uint16_t)len);
    CHECK(creates == 1 && connect_calls == 1);
    CHECK(connected_type == 1 && !memcmp(connected_addr, stadia, sizeof(bd_addr_t)));
    CHECK(device.conn.protocol == UNI_BT_CONN_PROTOCOL_BLE && device.conn.state == UNI_BT_CONN_STATE_DEVICE_DISCOVERED);
    CHECK(!memcmp(event, original, sizeof(event))); CHECK(db_reads == MAX_NR_LE_DEVICE_DB_ENTRIES);
    /* Repeated advertisements while a connection attempt owns the slot do not reconnect. */
    uni_bt_le_on_gap_event_advertising_report(event, (uint16_t)len); CHECK(connect_calls == 1);
    for (unsigned cycle = 0; cycle < 50; ++cycle) {
        allocated = false;
        len = advertisement(event, (uint8_t)(cycle & 1), 1, stadia, NULL, 0);
        uni_bt_le_on_gap_event_advertising_report(event, (uint16_t)len);
        CHECK(allocated && connect_calls == cycle + 2);
    }
    allocated = false; unsigned before = creates;
    /* Unknown addresses, different address types and nonconnectable advertisements cannot use the bond exception. */
    len = advertisement(event, 1, 1, unknown, NULL, 0);
    uni_bt_le_on_gap_event_advertising_report(event, (uint16_t)len); CHECK(creates == before);
    len = advertisement(event, 1, 0, stadia, NULL, 0);
    uni_bt_le_on_gap_event_advertising_report(event, (uint16_t)len); CHECK(creates == before);
    for (uint8_t type = 2; type <= 4; ++type) {
        len = advertisement(event, type, 1, stadia, NULL, 0);
        uni_bt_le_on_gap_event_advertising_report(event, (uint16_t)len); CHECK(creates == before);
    }
    /* New Stadia can identify itself in an active scan response without Appearance. */
    const uint8_t name[] = {11,BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME,'S','t','a','d','i','a','-','1','2','3'};
    len = advertisement(event, 4, 0, unknown, name, sizeof(name));
    uni_bt_le_on_gap_event_advertising_report(event, (uint16_t)len); CHECK(creates == before + 1);
    CHECK(!memcmp(connected_addr, unknown, sizeof(bd_addr_t)) && connected_type == 0);
    /* Ordinary Xbox/other valid HID reports still traverse the original path. */
    allocated = false; before = creates;
    const uint8_t hid[] = {3,BLUETOOTH_DATA_TYPE_APPEARANCE,0xc4,0x03};
    len = advertisement(event, 0, 0, unknown, hid, sizeof(hid));
    uni_bt_le_on_gap_event_advertising_report(event, (uint16_t)len); CHECK(creates == before + 1);
    allocated = false; before = creates;
    const uint8_t nonhid[] = {3,BLUETOOTH_DATA_TYPE_APPEARANCE,1,1};
    len = advertisement(event, 0, 1, stadia, nonhid, sizeof(nonhid));
    uni_bt_le_on_gap_event_advertising_report(event, (uint16_t)len); CHECK(creates == before);
    /* Discovery gate still prevents a second pad/connection during forget. */
    discovery_allowed = false;
    len = advertisement(event, 1, 1, stadia, NULL, 0);
    uni_bt_le_on_gap_event_advertising_report(event, (uint16_t)len); CHECK(creates == before);
    discovery_allowed = true;
    /* Invalid lengths and malformed AD are rejected before calling vendor parser. */
    uni_bt_le_on_gap_event_advertising_report(NULL, 0); CHECK(creates == before);
    for (uint16_t size = 0; size < 12; ++size) {
        uni_bt_le_on_gap_event_advertising_report(event, size); CHECK(creates == before);
    }
    event[11] = 32; uni_bt_le_on_gap_event_advertising_report(event, 43); CHECK(creates == before);
    event[11] = 4; event[1] = 14; uni_bt_le_on_gap_event_advertising_report(event, 12); CHECK(creates == before);
    const uint8_t invalid_ad[] = {7,BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME,'S'};
    len = advertisement(event, 0, 1, stadia, invalid_ad, sizeof(invalid_ad));
    uni_bt_le_on_gap_event_advertising_report(event, (uint16_t)len); CHECK(creates == before);
    /* A full-size wake packet stays bounded and input buffer stays untouched. */
    uint8_t full_ad[31] = {30,BLUETOOTH_DATA_TYPE_MANUFACTURER_SPECIFIC_DATA};
    len = advertisement(event, 0, 1, stadia, full_ad, sizeof(full_ad)); memcpy(original, event, sizeof(event));
    uni_bt_le_on_gap_event_advertising_report(event, (uint16_t)len);
    CHECK(creates == before + 1 && !memcmp(original, event, sizeof(event)));
    allocated = false; before = creates; receiver_ble_clear_cache();
    len = advertisement(event, 1, 1, stadia, NULL, 0);
    uni_bt_le_on_gap_event_advertising_report(event, (uint16_t)len); CHECK(creates == before);
    /* Identity-address type from the DB matches a public address report. */
    db_types[7] = BD_ADDR_TYPE_LE_PUBLIC_IDENTITY; receiver_ble_refresh_bonds();
    len = advertisement(event, 1, 0, stadia, NULL, 0);
    uni_bt_le_on_gap_event_advertising_report(event, (uint16_t)len); CHECK(creates == before + 1);
    printf("BLE discovery: %u checks passed (50 bonded wake cycles; original R2 rejects wake packet).\n", checks);
    return 0;
}
