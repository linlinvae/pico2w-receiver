param([string]$Compiler = 'gcc')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$out = Join-Path $root 'build/host-tests'
$sdk = Join-Path (Split-Path $root -Parent) 'receiver/sdk/CH583EVT/EVT/EXAM'
New-Item -ItemType Directory -Force -Path $out | Out-Null
& $Compiler -std=gnu99 -O2 -Wall -Wextra -Werror "-I$root/common" `
    "$root/tests/test_link.c" "$root/common/sp_link.c" "$root/common/sp_bridge.c" "$root/common/sp_endpoint.c" -o "$out/link.exe"
if ($LASTEXITCODE -ne 0) { throw 'Link test compilation failed' }
& "$out/link.exe"
if ($LASTEXITCODE -ne 0) { throw 'Link tests failed' }
& $Compiler -std=gnu99 -O2 -Wall -Wextra -Werror "-I$root/ch582/src" `
    "$root/tests/test_xinput_builder.c" "$root/ch582/src/xinput_builder.c" -o "$out/xinput.exe"
if ($LASTEXITCODE -ne 0) { throw 'XInput test compilation failed' }
& "$out/xinput.exe"
if ($LASTEXITCODE -ne 0) { throw 'XInput tests failed' }
& $Compiler -std=gnu99 -O2 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-function `
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast '-D__INTERRUPT=' '-D__HIGH_CODE=' -DCFG_CH582 `
    '-include' "$root/ch582/src/board_config.h" "-I$root/ch582/src" `
    "-I$sdk/SRC/RVMSIS" "-I$sdk/SRC/StdPeriphDriver/inc" "-I$sdk/BLE/LIB" `
    "$root/tests/test_usb_state.c" -o "$out/usb.exe"
if ($LASTEXITCODE -ne 0) { throw 'USB test compilation failed' }
& "$out/usb.exe"
if ($LASTEXITCODE -ne 0) { throw 'USB tests failed' }
& "$PSScriptRoot/verify-preserved.ps1"
Write-Output 'All host checks passed. Bluetooth, UART wiring and host enumeration still require hardware testing.'
