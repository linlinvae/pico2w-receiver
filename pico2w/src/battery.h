// SPDX-License-Identifier: Apache-2.0
#ifndef RECEIVER_BATTERY_H
#define RECEIVER_BATTERY_H
#include "uni.h"
#include "bridge.h"
#define RECEIVER_STATUS_SIZE 31
void receiver_battery_init(void);
void receiver_battery_attach(uni_hid_device_t *device, pad_kind_t kind, uint32_t now);
void receiver_battery_detach(void);
void receiver_battery_service(uint32_t now);
void receiver_battery_status(uint8_t out[RECEIVER_STATUS_SIZE]);
#endif
