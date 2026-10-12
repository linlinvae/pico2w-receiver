param([string]$Gcc='gcc')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
New-Item -ItemType Directory -Force "$root/tests/build" | Out-Null
$flags=@('-std=c11','-Wall','-Wextra','-Werror',"-I$root/src")
& $Gcc @flags "$root/src/receiver_config.c" "$root/src/bridge.c" "$root/tests/test_bridge.c" -o "$root/tests/build/test_bridge.exe"
if($LASTEXITCODE -ne 0) {throw 'Bridge test compilation failed'}
& "$root/tests/build/test_bridge.exe"
if($LASTEXITCODE -ne 0) {throw 'Bridge tests failed'}
& $Gcc @flags -DCFG_TUSB_MCU=OPT_MCU_RP2040 "-I$root/tests/stubs" "-I$root/resources/pico-sdk/lib/tinyusb/src" `
    "$root/src/receiver_config.c" "$root/src/config_protocol.c" "$root/src/bridge.c" "$root/src/usb_xinput.c" "$root/src/usb_descriptors.c" "$root/tests/test_usb.c" `
    "$root/resources/pico-sdk/lib/tinyusb/src/class/hid/hid_device.c" `
    -o "$root/tests/build/test_usb.exe"
if($LASTEXITCODE -ne 0) {throw 'USB test compilation failed'}
& "$root/tests/build/test_usb.exe"
if($LASTEXITCODE -ne 0) {throw 'USB tests failed'}
$btroot="$root/resources/bluepad32/external/btstack"
$btflags=@('-DENABLE_BLE','-DENABLE_CLASSIC',"-I$root/tests/stubs",
    "-I$root/resources/bluepad32/src/components/bluepad32/include","-I$btroot/src",
    "-I$btroot/3rd-party/bluedroid/encoder/include","-I$btroot/3rd-party/bluedroid/decoder/include",
    "-I$btroot/3rd-party/yxml")
& $Gcc @flags @btflags "$root/src/battery.c" "$root/tests/test_battery.c" "$btroot/src/btstack_util.c" -o "$root/tests/build/test_battery.exe"
if($LASTEXITCODE -ne 0) {throw 'Battery test compilation failed'}
& "$root/tests/build/test_battery.exe"
if($LASTEXITCODE -ne 0) {throw 'Battery tests failed'}
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot/battery-monitor.ps1" -SelfTest
if($LASTEXITCODE -ne 0) {throw 'Battery monitor protocol tests failed'}
& "$PSScriptRoot/make-classic-test-fixture.ps1"
& $Gcc @flags @btflags `
    "$root/src/receiver_config.c" "$root/src/config_protocol.c" "$root/src/bridge.c" "$root/src/platform.c" "$root/tests/test_platform.c" `
    "$root/tests/build/vendor_classic_incoming.c" "$btroot/src/btstack_util.c" `
    '-Wl,--wrap=uni_bt_bredr_on_l2cap_incoming_connection' `
    -o "$root/tests/build/test_platform.exe"
if($LASTEXITCODE -ne 0) {throw 'Platform test compilation failed'}
& "$root/tests/build/test_platform.exe"
if($LASTEXITCODE -ne 0) {throw 'Platform reconnect tests failed'}
& $Gcc @flags @btflags `
    "$root/src/receiver_config.c" "$root/src/config_protocol.c" "$root/src/bridge.c" "$root/src/platform.c" "$root/tests/test_config_platform.c" `
    "$root/tests/build/vendor_classic_incoming.c" "$btroot/src/btstack_util.c" `
    '-Wl,--wrap=uni_bt_bredr_on_l2cap_incoming_connection' -o "$root/tests/build/test_config_platform.exe"
if($LASTEXITCODE -ne 0) {throw 'Config platform test compilation failed'}
foreach($case in @('connected','pairing','success','failure','timeout')) {
    & "$root/tests/build/test_config_platform.exe" $case
    if($LASTEXITCODE -ne 0) {throw "Config platform tests failed: $case"}
}
& "$PSScriptRoot/make-ble-test-fixture.ps1"
& $Gcc @flags @btflags -ffunction-sections -fdata-sections `
    "$root/src/ble_discovery.c" "$root/tests/test_ble_discovery.c" `
    "$root/tests/build/vendor_ble_discovery.c" `
    "$btroot/src/btstack_util.c" "$btroot/src/ad_parser.c" `
    '-Wl,--gc-sections' '-Wl,--wrap=uni_bt_le_on_gap_event_advertising_report' `
    -o "$root/tests/build/test_ble_discovery.exe"
if($LASTEXITCODE -ne 0) {throw 'BLE discovery test compilation failed'}
& "$root/tests/build/test_ble_discovery.exe"
if($LASTEXITCODE -ne 0) {throw 'BLE discovery tests failed'}
& $Gcc @flags "-I$root/tests/stubs" "$root/src/receiver_config.c" "$root/src/config_flash.c" "$root/src/config_protocol.c" "$root/src/bridge.c" "$root/tests/test_config.c" -o "$root/tests/build/test_config.exe"
if($LASTEXITCODE -ne 0) {throw 'Config test compilation failed'}
& "$root/tests/build/test_config.exe" "$root/tests/build/config-vectors.bin"
if($LASTEXITCODE -ne 0) {throw 'Config tests failed'}
