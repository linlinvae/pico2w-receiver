param([Parameter(Mandatory=$true)][string]$Port,
      [ValidateRange(9600,921600)][int]$Baud = 460800)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$python = Join-Path $root 'resources/idf-tools/python_env/idf5.5_py3.12_env/Scripts/python.exe'
$out = Join-Path $root 'firmware/ESP32-SwitchPro-r1'
foreach ($file in @($python,"$out/bootloader.bin","$out/partition-table.bin","$out/switchpro_esp32_r1.bin","$out/build-info.json")) {
    if (!(Test-Path -LiteralPath $file)) { throw "Required file missing: $file" }
}
$info = Get-Content "$out/build-info.json" -Raw | ConvertFrom-Json
if (@($info.segmentHashes).Count -ne 3) { throw 'Segment checksums missing. Rebuild the firmware.' }
foreach ($segment in $info.segmentHashes) {
    if ($segment.file -notin @('bootloader.bin','partition-table.bin','switchpro_esp32_r1.bin') -or
        (Get-FileHash "$out/$($segment.file)").Hash -ne $segment.sha256) {
        throw "Firmware checksum mismatch: $($segment.file)"
    }
}
# Explicit port only. Flash separate segments, preserving the NVS gap.
& $python -m esptool --chip esp32 --port $Port --baud $Baud write_flash `
    --flash_mode dio --flash_freq 40m --flash_size 4MB `
    0x1000 "$out/bootloader.bin" 0x8000 "$out/partition-table.bin" 0x10000 "$out/switchpro_esp32_r1.bin"
if ($LASTEXITCODE -ne 0) { throw 'ESP32 flash failed' }
