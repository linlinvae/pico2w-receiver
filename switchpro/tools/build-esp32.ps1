param([string]$IdfPath, [string]$IdfToolsPath, [string]$Python,
      [ValidateSet('build','menuconfig')][string]$Action = 'build')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (!$IdfPath) { $IdfPath = Join-Path $root 'resources/esp-idf' }
if (!$IdfToolsPath) { $IdfToolsPath = Join-Path $root 'resources/idf-tools' }
if (!$Python) { $Python = Join-Path $IdfToolsPath 'python_env/idf5.5_py3.12_env/Scripts/python.exe' }
if (!(Test-Path -LiteralPath $Python)) { throw 'ESP-IDF Python environment missing. Run tools/setup-esp32.ps1.' }
$env:IDF_PATH = (Resolve-Path $IdfPath).Path
$env:IDF_TOOLS_PATH = (Resolve-Path $IdfToolsPath).Path
$env:IDF_PYTHON_ENV_PATH = Split-Path (Split-Path $Python -Parent) -Parent
$env:IDF_TARGET = 'esp32'
# Selected SDK submodules are checked/pinned by setup. Unused SoC/test dependencies are not needed.
$env:IDF_SKIP_CHECK_SUBMODULES = '1'
$env:IDF_COMPONENT_MANAGER = '0'
New-Item -ItemType Directory -Force -Path "$root/resources/tmp" | Out-Null
$env:TEMP = (Resolve-Path "$root/resources/tmp").Path
$env:TMP = $env:TEMP
$paths = @((Split-Path $Python -Parent),
    "$IdfToolsPath/tools/xtensa-esp-elf/esp-14.2.0_20241119/xtensa-esp-elf/bin",
    "$IdfToolsPath/tools/esp32ulp-elf/2.38_20240113/esp32ulp-elf/bin",
    "$IdfToolsPath/tools/cmake/3.30.2/bin",
    "$IdfToolsPath/tools/ninja/1.12.1")
foreach ($path in $paths) { if (!(Test-Path -LiteralPath $path)) { throw "Required tool missing: $path" } }
$env:PATH = ($paths -join ';') + ';' + $env:PATH
$build = Join-Path $root 'build/ESP32-SwitchPro-r1'
$project = Join-Path $root 'esp32'
& $Python "$IdfPath/tools/idf.py" -C $project -B $build '-DIDF_TARGET=esp32' `
    "-DSDKCONFIG=$build/sdkconfig" "-DSDKCONFIG_DEFAULTS=$project/sdkconfig.defaults" $Action
if ($LASTEXITCODE -ne 0) { throw 'ESP32 build failed' }
if ($Action -ne 'build') { return }
$out = Join-Path $root 'firmware/ESP32-SwitchPro-r1'
New-Item -ItemType Directory -Force -Path $out | Out-Null
Copy-Item -LiteralPath "$build/switchpro_esp32_r1.bin","$build/switchpro_esp32_r1.elf",`
    "$build/switchpro_esp32_r1.map","$build/sdkconfig" -Destination $out -Force
Copy-Item -LiteralPath "$build/bootloader/bootloader.bin" -Destination "$out/bootloader.bin" -Force
Copy-Item -LiteralPath "$build/partition_table/partition-table.bin" -Destination "$out/partition-table.bin" -Force
$flashArgs = Get-Content "$build/flasher_args.json" -Raw | ConvertFrom-Json
foreach ($entry in $flashArgs.flash_files.PSObject.Properties) {
    $entry.Value = [IO.Path]::GetFileName($entry.Value)
}
foreach ($segment in @('bootloader','app','partition-table')) {
    $flashArgs.$segment.file = [IO.Path]::GetFileName($flashArgs.$segment.file)
}
$flashArgs | ConvertTo-Json -Depth 7 | Set-Content "$out/flasher_args.json" -Encoding utf8
& $Python -m esptool --chip esp32 merge_bin -o "$out/switchpro-ESP32-r1-full.bin" `
    --flash_mode dio --flash_freq 40m --flash_size 4MB `
    0x1000 "$out/bootloader.bin" 0x8000 "$out/partition-table.bin" 0x10000 "$out/switchpro_esp32_r1.bin"
if ($LASTEXITCODE -ne 0) { throw 'ESP32 merged image failed' }
[ordered]@{chip='original ESP32'; revision=1; flashMegabytes=4; binaryFlashAddress='0x0';
    binarySha256=(Get-FileHash "$out/switchpro-ESP32-r1-full.bin").Hash;
    segmentHashes=@('bootloader.bin','partition-table.bin','switchpro_esp32_r1.bin' | ForEach-Object {
        @{file=$_;sha256=(Get-FileHash "$out/$_").Hash}
    });
    dependencies=(Get-Content "$root/dependencies.lock.json" -Raw | ConvertFrom-Json);
    sourceHashes=@(Get-ChildItem "$project/main/*","$root/common/*","$project/CMakeLists.txt","$project/sdkconfig.defaults" -File | ForEach-Object { @{path=$_.FullName;sha256=(Get-FileHash $_.FullName).Hash} })
} | ConvertTo-Json -Depth 7 | Set-Content "$out/build-info.json" -Encoding utf8
Write-Output "Built: $out/switchpro-ESP32-r1-full.bin (flash at 0x0)"
