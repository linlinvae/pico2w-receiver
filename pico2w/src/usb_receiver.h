// SPDX-License-Identifier: Apache-2.0
#ifndef RECEIVER_USB_RECEIVER_H
#define RECEIVER_USB_RECEIVER_H
/* Keep Bluetooth, configuration and reconnect logic independent of USB mode. */
#ifdef RECEIVER_USB_SWITCH
#include "usb_switch.h"
#define receiver_usb_init usb_switch_init
#define receiver_usb_service usb_switch_service
#define receiver_usb_config usb_switch_config
#define RECEIVER_PLATFORM_NAME "Pico2W-Switch-R1"
#else
#include "usb_xinput.h"
#define receiver_usb_init usb_xinput_init
#define receiver_usb_service usb_xinput_service
#define receiver_usb_config usb_xinput_config
#define RECEIVER_PLATFORM_NAME "Pico2W-XInput-R5"
#endif
#endif
