param([string]$Python='python', [int]$Jobs=8, [string]$BuildDirectory='')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
& "$PSScriptRoot/verify-dependencies.ps1"
& "$PSScriptRoot/verify-r1.ps1"
& "$PSScriptRoot/verify-r2.ps1"
& "$PSScriptRoot/verify-r3.ps1"
& "$PSScriptRoot/verify-r4.ps1"
& "$PSScriptRoot/verify-r4-1.ps1"
& "$PSScriptRoot/verify-r4-2.ps1"
& "$PSScriptRoot/verify-r5.ps1"
if ((Test-Path "$root/../receiver") -or (Test-Path "$root/../switchpro")) {
    & "$PSScriptRoot/verify-preserved.ps1"
} else { Write-Output 'Standalone checkout: previous projects absent; preservation audit not applicable.' }
$cmake=Join-Path $root 'resources/cmake-download/cmake-3.30.2-windows-x86_64/bin/cmake.exe'
if(!(Test-Path $cmake)) { $cmake=Join-Path $root 'resources/cmake/bin/cmake.exe' }
$ninja=Join-Path $root 'resources/ninja/ninja.exe'
$env:PICO_TOOLCHAIN_PATH=Join-Path $root 'resources/toolchain'
$env:Path=(Join-Path $root 'resources/toolchain/bin')+';'+$env:Path
if(!$BuildDirectory) { $BuildDirectory=Join-Path $root 'build' }
New-Item -ItemType Directory -Force $BuildDirectory,"$root/firmware" | Out-Null
& $cmake -S $root -B $BuildDirectory -G Ninja -DCMAKE_BUILD_TYPE=Release -DRECEIVER_USB_MODE=xinput `
    "-DCMAKE_MAKE_PROGRAM=$ninja" "-DPython3_EXECUTABLE=$Python" `
    "-Dpicotool_DIR=$root/resources/picotool/picotool" "-Dpioasm_DIR=$root/resources/sdk-tools/pioasm" `
    "-DPICO_SDK_PATH=$root/resources/pico-sdk" "-DBLUEPAD32_ROOT=$root/resources/bluepad32" `
    "-DPICO_BTSTACK_PATH=$root/resources/bluepad32/external/btstack" *> "$BuildDirectory/configure.log"
if($LASTEXITCODE -ne 0) {Get-Content "$BuildDirectory/configure.log" -Tail 40; throw 'Configure failed'}
& $cmake --build $BuildDirectory -j $Jobs *> "$BuildDirectory/build.log"
if($LASTEXITCODE -ne 0) {Get-Content "$BuildDirectory/build.log" -Tail 40; throw 'Build failed'}
& $Python "$PSScriptRoot/verify_uf2.py" "$BuildDirectory/pico2w_receiver.uf2"
if($LASTEXITCODE -ne 0) {throw 'UF2 validation failed'}
$base=Join-Path $root 'firmware/pico2w-xinput-r5-1'
foreach($ext in @('uf2','elf','bin')) {Copy-Item "$BuildDirectory/pico2w_receiver.$ext" "$base.$ext" -Force}
$sources=@(Get-ChildItem "$root/src","$root/tools" -File)+@(Get-ChildItem "$root/tests" -Recurse -File | Where-Object FullName -NotMatch '[\\/]build[\\/]')+@(Get-Item "$root/CMakeLists.txt","$root/flash_guard.ld","$root/dependencies.lock.json")
$sourceHashes=@($sources | ForEach-Object { [PSCustomObject]@{path=$_.FullName.Substring($root.Length+1);sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash} })
$outputs=@(Get-Item "$base.uf2","$base.elf","$base.bin" | ForEach-Object { [PSCustomObject]@{file=$_.Name;bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash} })
[PSCustomObject]@{version='R5.1';board='pico2_w';platform='rp2350-arm-s';builtUtc=[DateTime]::UtcNow.ToString('o');
    hardwareValidated=$false;dependencies=(Get-Content "$root/dependencies.lock.json" -Raw | ConvertFrom-Json);
    sourceHashes=$sourceHashes;outputs=$outputs} | ConvertTo-Json -Depth 10 | Set-Content "$base-build-info.json" -Encoding utf8
& "$root/resources/toolchain/bin/arm-none-eabi-size.exe" "$base.elf"
Write-Output "Firmware ready: $base.uf2"
