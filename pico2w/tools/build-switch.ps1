# SPDX-License-Identifier: Apache-2.0
param([string]$Python='python', [int]$Jobs=8, [string]$BuildDirectory='')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
& "$PSScriptRoot/verify-dependencies.ps1"
foreach($version in @('r1','r2','r3','r4','r4-1','r4-2','r5')) { & "$PSScriptRoot/verify-$version.ps1" }
if ((Test-Path "$root/../receiver") -or (Test-Path "$root/../switchpro")) {
    & "$PSScriptRoot/verify-preserved.ps1"
}
$cmake=Join-Path $root 'resources/cmake-download/cmake-3.30.2-windows-x86_64/bin/cmake.exe'
if(!(Test-Path $cmake)) { $cmake=Join-Path $root 'resources/cmake/bin/cmake.exe' }
$ninja=Join-Path $root 'resources/ninja/ninja.exe'
if(!$BuildDirectory) { $BuildDirectory=Join-Path $root 'build/switch-r1-1' }
$build=$BuildDirectory
$env:PICO_TOOLCHAIN_PATH=Join-Path $root 'resources/toolchain'
$env:Path=(Join-Path $root 'resources/toolchain/bin')+';'+$env:Path
New-Item -ItemType Directory -Force $build,"$root/firmware" | Out-Null
& $cmake -S $root -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release -DRECEIVER_USB_MODE=switch `
    "-DCMAKE_MAKE_PROGRAM=$ninja" "-DPython3_EXECUTABLE=$Python" `
    "-Dpicotool_DIR=$root/resources/picotool/picotool" "-Dpioasm_DIR=$root/resources/sdk-tools/pioasm" `
    "-DPICO_SDK_PATH=$root/resources/pico-sdk" "-DBLUEPAD32_ROOT=$root/resources/bluepad32" `
    "-DPICO_BTSTACK_PATH=$root/resources/bluepad32/external/btstack" *> "$build/configure.log"
if($LASTEXITCODE -ne 0) { Get-Content "$build/configure.log" -Tail 40; throw 'Switch configure failed' }
& $cmake --build $build -j $Jobs *> "$build/build.log"
if($LASTEXITCODE -ne 0) { Get-Content "$build/build.log" -Tail 40; throw 'Switch build failed' }
& $Python "$PSScriptRoot/verify_uf2.py" "$build/pico2w_receiver.uf2"
if($LASTEXITCODE -ne 0) { throw 'Switch UF2 validation failed' }
$base=Join-Path $root 'firmware/pico2w-switch-r1-1'
foreach($ext in @('uf2','elf','bin')) { Copy-Item -LiteralPath "$build/pico2w_receiver.$ext" -Destination "$base.$ext" -Force }
$sources=@(Get-ChildItem "$root/src","$root/tools","$root/licenses","$root/docs" -File)+
    @(Get-ChildItem "$root/tests" -Recurse -File | Where-Object FullName -NotMatch '[\\/]build[\\/]')+
    @(Get-Item "$root/CMakeLists.txt","$root/flash_guard.ld","$root/dependencies.lock.json","$root/NOTICE.md")
$sourceHashes=@($sources | ForEach-Object { [PSCustomObject]@{path=$_.FullName.Substring($root.Length+1);sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash} })
$outputs=@(Get-Item "$base.uf2","$base.elf","$base.bin" | ForEach-Object { [PSCustomObject]@{file=$_.Name;bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash} })
[PSCustomObject]@{version='Switch-R1.1';usbMode='switch-wired-hid';board='pico2_w';platform='rp2350-arm-s';
    activeControllers=1;controllerTypes=@('Xbox Series 045E:0B13 BLE','Switch Pro 057E:2009 Classic','Stadia 18D1:9400 BLE');
    usbIdentity='0F0D:0092';inputIntervalMs=1;rumbleSupported=$false;gyroSupported=$false;
    builtUtc=[DateTime]::UtcNow.ToString('o');hardwareValidated=$false;
    dependencies=(Get-Content "$root/dependencies.lock.json" -Raw | ConvertFrom-Json);
    sourceHashes=$sourceHashes;outputs=$outputs} | ConvertTo-Json -Depth 10 | Set-Content "$base-build-info.json" -Encoding utf8
& "$PSScriptRoot/verify-r5.ps1"
& "$root/resources/toolchain/bin/arm-none-eabi-size.exe" "$base.elf"
if($LASTEXITCODE -ne 0) { throw 'Firmware size inspection failed' }
Write-Output "Switch firmware ready: $base.uf2"
