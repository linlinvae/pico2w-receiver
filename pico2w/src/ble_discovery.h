// SPDX-License-Identifier: Apache-2.0
#ifndef RECEIVER_BLE_DISCOVERY_H
#define RECEIVER_BLE_DISCOVERY_H
/* Called on the foreground run loop after LE database initialization/pairing. */
void receiver_ble_refresh_bonds(void);
void receiver_ble_clear_cache(void);
#endif
