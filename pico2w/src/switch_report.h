// SPDX-License-Identifier: Apache-2.0
#ifndef RECEIVER_SWITCH_REPORT_H
#define RECEIVER_SWITCH_REPORT_H
#include "bridge.h"
#define SWITCH_REPORT_SIZE 8
/* Convert the configured common report to Switch wired HID (no Report ID).
 * LT/RT become digital ZL/ZR at 50%; face buttons follow physical position. */
void switch_report(const bridge_t *bridge, uint8_t out[SWITCH_REPORT_SIZE]);
#endif
