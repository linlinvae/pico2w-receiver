# SPDX-License-Identifier: Apache-2.0
param([string]$Gcc='gcc')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
New-Item -ItemType Directory -Force "$root/tests/build" | Out-Null
$flags=@('-std=c11','-Wall','-Wextra','-Werror',"-I$root/src",'-DRECEIVER_USB_SWITCH=1')
& $Gcc @flags -DCFG_TUSB_MCU=OPT_MCU_RP2040 "-I$root/tests/stubs" "-I$root/resources/pico-sdk/lib/tinyusb/src" `
    "$root/src/receiver_config.c" "$root/src/config_protocol.c" "$root/src/bridge.c" `
    "$root/src/switch_report.c" "$root/src/usb_switch.c" "$root/src/usb_switch_descriptors.c" `
    "$root/tests/test_switch.c" "$root/resources/pico-sdk/lib/tinyusb/src/class/hid/hid_device.c" `
    -o "$root/tests/build/test_switch.exe"
if($LASTEXITCODE -ne 0) {throw 'Switch USB test compilation failed'}
& "$root/tests/build/test_switch.exe"
if($LASTEXITCODE -ne 0) {throw 'Switch USB tests failed'}
# Re-run the same Bluetooth lifecycle tests against the Switch backend selection.
& "$PSScriptRoot/make-classic-test-fixture.ps1"
$btroot="$root/resources/bluepad32/external/btstack"
& $Gcc @flags -DENABLE_BLE -DENABLE_CLASSIC "-I$root/tests/stubs" `
    "-I$root/resources/bluepad32/src/components/bluepad32/include" "-I$btroot/src" `
    "-I$btroot/3rd-party/bluedroid/encoder/include" "-I$btroot/3rd-party/bluedroid/decoder/include" "-I$btroot/3rd-party/yxml" `
    "$root/src/receiver_config.c" "$root/src/config_protocol.c" "$root/src/bridge.c" "$root/src/platform.c" `
    "$root/tests/test_platform.c" "$root/tests/build/vendor_classic_incoming.c" "$btroot/src/btstack_util.c" `
    '-Wl,--wrap=uni_bt_bredr_on_l2cap_incoming_connection' -o "$root/tests/build/test_platform_switch.exe"
if($LASTEXITCODE -ne 0) {throw 'Switch Bluetooth platform test compilation failed'}
& "$root/tests/build/test_platform_switch.exe"
if($LASTEXITCODE -ne 0) {throw 'Switch Bluetooth platform tests failed'}
