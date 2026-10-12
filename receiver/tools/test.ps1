param(
    [string]$Compiler = 'gcc',
    [ValidateSet('CH582','CH592','all')][string]$Chip = 'CH582'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$testDir = Join-Path $projectRoot 'build/host-tests'
New-Item -ItemType Directory -Path $testDir -Force | Out-Null
$cases = @('stadia_protocol', 'sinput_builder', 'xinput_builder', 'rumble_queue', 'peer_store', 'button_gesture')
foreach ($case in $cases) {
    $binary = Join-Path $testDir ($case + '.exe')
    & $Compiler '-std=gnu99' '-Wall' '-Wextra' '-Werror' '-O2' "-I$projectRoot/src" `
        "$projectRoot/tests/test_$case.c" "$projectRoot/src/$case.c" '-o' $binary
    if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $case" }
    & $binary
    if ($LASTEXITCODE -ne 0) { throw "Tests failed: $case" }
}
$testChips = if ($Chip -eq 'all') { @('CH582','CH592') } else { @($Chip) }
foreach ($testChip in $testChips) {
    $sdkName = if ($testChip -eq 'CH582') { 'CH583EVT' } else { 'CH592EVT' }
    $sdk = "$projectRoot/sdk/$sdkName/EVT/EXAM"
if (Test-Path "$sdk/BLE/LIB") {
    Write-Host "Testing $testChip BLE/USB state machines"
    foreach ($state in @('ble_state','usb_state','usb_xinput')) {
    $stateSource = if ($state -eq 'usb_xinput') { 'usb_state' } else { $state }
    $modeFlags = @(if ($state -eq 'usb_xinput') { '-DUSB_GAMEPAD_XINPUT=1' })
    $binary = Join-Path $testDir "${testChip}_$state.exe"
    & $Compiler '-std=gnu99' '-O2' '-Wall' '-Wextra' '-Werror' '-Wno-unused-parameter' `
        '-Wno-unused-function' '-Wno-pointer-to-int-cast' '-Wno-int-to-pointer-cast' `
        '-D__INTERRUPT=' '-D__HIGH_CODE=' "-DCFG_$testChip" '-DBLE_MEMHEAP_SIZE=8192' "-I$projectRoot/src" `
        '-include' "$projectRoot/src/board_config.h" `
        "-I$sdk/SRC/RVMSIS" "-I$sdk/SRC/StdPeriphDriver/inc" "-I$sdk/BLE/HAL/include" "-I$sdk/BLE/LIB" `
        @modeFlags "$projectRoot/tests/test_$stateSource.c" '-o' $binary
    if ($LASTEXITCODE -ne 0) { throw "$state test compilation failed." }
    & $binary
    if ($LASTEXITCODE -ne 0) { throw "$state tests failed." }
    if ($state -eq 'ble_state') {
        $diagnosticBinary = Join-Path $testDir "${testChip}_ble_state_scan_diag.exe"
        & $Compiler '-std=gnu99' '-O2' '-Wall' '-Wextra' '-Werror' '-Wno-unused-parameter' `
            '-Wno-unused-function' '-Wno-pointer-to-int-cast' '-Wno-int-to-pointer-cast' `
            '-D__INTERRUPT=' '-D__HIGH_CODE=' "-DCFG_$testChip" '-DBLE_MEMHEAP_SIZE=8192' `
            '-include' "$projectRoot/src/board_config.h" `
            '-DSTADIA_SCAN_DIAGNOSTICS=1' "-I$projectRoot/src" "-I$sdk/SRC/RVMSIS" `
            "-I$sdk/SRC/StdPeriphDriver/inc" "-I$sdk/BLE/HAL/include" "-I$sdk/BLE/LIB" `
            "$projectRoot/tests/test_ble_state.c" '-o' $diagnosticBinary
        if ($LASTEXITCODE -ne 0) { throw 'Diagnostic BLE state test compilation failed.' }
        & $diagnosticBinary
        if ($LASTEXITCODE -ne 0) { throw 'Diagnostic BLE state tests failed.' }
    }
    }
} else {
    throw "BLE/USB state tests require the $testChip SDK; run tools/setup.ps1 -Chip $testChip."
}
}
Write-Host 'All available host tests passed. Hardware operation is not verified by these tests.'
