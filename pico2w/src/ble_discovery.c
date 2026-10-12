// SPDX-License-Identifier: Apache-2.0
#include <stdbool.h>
#include <string.h>
#include "sdkconfig.h"
#include "btstack.h"
#include "ble/le_device_db.h"
#include "bt/uni_bt_defines.h"
#include "ble_discovery.h"

/* Bluepad32 only discovers BLE HID peripherals when Appearance is present in
 * each advertising report. Bonded wake-up/directed packets need not include it.
 * Cache identities from the existing key database; no additional flash writes. */
static struct { uint8_t type; bd_addr_t address; } bonds[MAX_NR_LE_DEVICE_DB_ENTRIES];
static unsigned bond_count;
void receiver_ble_clear_cache(void) { bond_count = 0; }
void receiver_ble_refresh_bonds(void) {
    bond_count = 0;
    for (int i = 0; i < le_device_db_max_count() && bond_count < MAX_NR_LE_DEVICE_DB_ENTRIES; ++i) {
        int type;
        bd_addr_t addr;
        le_device_db_info(i, &type, addr, NULL);
        if (type != BD_ADDR_TYPE_LE_PUBLIC && type != BD_ADDR_TYPE_LE_RANDOM &&
            type != BD_ADDR_TYPE_LE_PUBLIC_IDENTITY && type != BD_ADDR_TYPE_LE_RANDOM_IDENTITY) continue;
        bonds[bond_count].type = (uint8_t)(type & 1);
        memcpy(bonds[bond_count++].address, addr, sizeof(bd_addr_t));
    }
}
static bool bonded(const bd_addr_t addr, uint8_t type) {
    if (type > BD_ADDR_TYPE_LE_RANDOM_IDENTITY) return false;
    for (unsigned i = 0; i < bond_count; ++i)
        if (bonds[i].type == (type & 1) && !memcmp(bonds[i].address, addr, sizeof(bd_addr_t))) return true;
    return false;
}
void __real_uni_bt_le_on_gap_event_advertising_report(const uint8_t *packet, uint16_t size);
void __wrap_uni_bt_le_on_gap_event_advertising_report(const uint8_t *packet, uint16_t size) {
    if (!packet || size < 12 || packet[0] != GAP_EVENT_ADVERTISING_REPORT) return;
    unsigned len = gap_event_advertising_report_get_data_length(packet);
    if (len > 31 || len > (unsigned)size - 12 || packet[1] != len + 10) return;
    const uint8_t *ad = gap_event_advertising_report_get_data(packet);
    uint16_t appearance = 0;
    bool stadia_name = false;
    for (unsigned offset = 0; offset < len;) {
        unsigned field_len = ad[offset];
        if (!field_len) break;
        if (field_len > len - offset - 1) return;
        uint8_t field_type = ad[offset + 1];
        if (field_type == BLUETOOTH_DATA_TYPE_APPEARANCE && field_len >= 3)
            appearance = little_endian_read_16(ad, offset + 2);
        if ((field_type == BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME ||
             field_type == BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME) && field_len >= 7 &&
            !memcmp(ad + offset + 2, "Stadia", 6)) stadia_name = true;
        offset += field_len + 1;
    }
    bd_addr_t addr;
    gap_event_advertising_report_get_address(packet, addr);
    uint8_t event_type = gap_event_advertising_report_get_advertising_event_type(packet);
    bool connectable = event_type == 0 /* ADV_IND */ || event_type == 1 /* ADV_DIRECT_IND */;
    bool known_peer = connectable && bonded(addr, gap_event_advertising_report_get_address_type(packet));
    bool new_stadia = stadia_name && (event_type == 0 || event_type == 4 /* SCAN_RSP */);
    if (appearance == 0 && (known_peer || new_stadia)) {
        /* Supply discovery metadata only. GAP address/type/RSSI are unchanged;
         * encryption and real GATT VID/PID/parser validation still run normally.
         * The incoming packet is stack-owned and must never be modified. */
        uint8_t adapted[16];
        memcpy(adapted, packet, 12);
        adapted[1] = 14; adapted[11] = 4;
        adapted[12] = 3; adapted[13] = BLUETOOTH_DATA_TYPE_APPEARANCE;
        little_endian_store_16(adapted, 14, UNI_BT_HID_APPEARANCE_GAMEPAD);
        __real_uni_bt_le_on_gap_event_advertising_report(adapted, sizeof(adapted));
        return;
    }
    __real_uni_bt_le_on_gap_event_advertising_report(packet, size);
}
