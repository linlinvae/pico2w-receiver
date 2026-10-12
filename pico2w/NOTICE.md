# Notices

Application sources in src/ (except board_button.c and usb_switch_descriptors.*): original local receiver implementation, Apache License 2.0; PC USB layouts follow Microsoft MS-XUSBI. Mapping derives from this workspace's independent switchpro/common/sp_link.c implementation. No existing CH582F/ESP32 firmware was modified.

Switch R1 usb_switch_descriptors.c/.h: MIT, Copyright (c) 2021 Jason Skuby (mytechtoybox.com), adapted protocol descriptors from OpenStickCommunity/GP2040-CE, headers/drivers/switch/SwitchDescriptors.h. Full license: licenses/GP2040-Switch.txt. Upstream reference: https://github.com/OpenStickCommunity/GP2040-CE/blob/main/headers/drivers/switch/SwitchDescriptors.h (read 2026-10-11). USB identity 0F0D:0092 is the established Switch wired HID emulation profile; manufacturer/product strings identify this local receiver. This project is not affiliated with HORI or Nintendo. No GP2040 runtime library is linked.

board_button.c: Copyright (c) 2020 Raspberry Pi (Trading) Ltd., BSD-3-Clause, adapted from pico-examples/picoboard/button/button.c. The full BSD-3-Clause license is in licenses/PicoSDK.txt.

btstack_config.h: adapted from the Bluepad32 public-domain Pico W example, which references Raspberry Pi pico-examples configuration.

Bluepad32: Copyright Ricardo Quesada and contributors, Apache-2.0; licenses/Bluepad32.txt.

R3 ble_discovery.c wraps the pinned Bluepad32 discovery entry point at link time; dependency source files remain unchanged. Host tests generate an ignored fixture containing the exact vendor discovery function and helpers from uni_bt_le.c, under the notices and license of that dependency. Generated fixtures and downloaded dependencies are not included in release ZIPs.

BTstack: Copyright BlueKitchen GmbH and contributors; licenses/BTstack.txt and licenses/BTstack-RaspberryPi.txt.

Pico SDK / Pico examples: Copyright Raspberry Pi (Trading) Ltd. and contributors, BSD-3-Clause; licenses/PicoSDK.txt.

TinyUSB: Copyright Ha Thach and contributors, MIT; licenses/TinyUSB.txt.

CYW43 driver and firmware: Copyright George Robotics Pty Ltd., Infineon/Cypress and contributors; licenses/CYW43.txt and licenses/CYW43-RaspberryPi.txt. Retain upstream individual file and firmware notices when redistributing the dependency source tree.

The attached Raspberry Pi BTstack and CYW43 licenses apply to the Pico 2 W target. The downloaded compiler and host tools retain their own upstream licenses inside resources/; they are not linked into application code.
