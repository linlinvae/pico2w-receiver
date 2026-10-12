param([string]$ToolchainBin, [string]$SdkBase, [ValidateSet(0,1)][int]$UartRemap = 0)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$receiver = Join-Path (Split-Path $root -Parent) 'receiver'
if (!$ToolchainBin) { $ToolchainBin = Join-Path $receiver 'resources/wch-gcc/bin' }
if (!$SdkBase) { $SdkBase = Join-Path $receiver 'sdk/CH583EVT/EVT/EXAM' }
$prefix = Join-Path $ToolchainBin 'riscv-none-embed-'
$cc = $prefix + 'gcc.exe'
if (!(Test-Path $cc)) { throw 'WCH compiler missing. Specify -ToolchainBin.' }
$out = Join-Path $root 'firmware/CH582-SwitchPro-r1'
$build = Join-Path $root 'build/CH582-SwitchPro-r1'
New-Item -ItemType Directory -Force -Path $out,$build | Out-Null
$ld = Join-Path $build 'Link.ld'
$text = [IO.File]::ReadAllText("$SdkBase/SRC/Ld/Link.ld")
if ($text -notmatch '__stack_size\s*=\s*\d+\s*;') { throw 'Unsupported linker script' }
$text = $text -replace '__stack_size\s*=\s*\d+\s*;', '__stack_size = 2048;'
$text += "`nASSERT(_ebss <= _susrstack, `"RAM overlaps the reserved stack`");`n"
[IO.File]::WriteAllText($ld,$text,[Text.UTF8Encoding]::new($false))
$cpu = @('-march=rv32imac','-mabi=ilp32','-mcmodel=medany','-msmall-data-limit=8','-mno-save-restore')
$flags = $cpu + @('-Os','-ffunction-sections','-fdata-sections','-g','-gdwarf-2',
    '-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-unused-function','-std=gnu99',
    '-DCFG_CH582','-DISP582','-DCH58xBLE',"-DBOARD_UART_REMAP=$UartRemap",
    '-include',"$root/ch582/src/board_config.h","-I$root/ch582/src","-I$root/common",
    "-I$SdkBase/SRC/RVMSIS","-I$SdkBase/SRC/StdPeriphDriver/inc","-I$SdkBase/BLE/LIB")
$sources = @(Get-ChildItem "$root/ch582/src/*.c","$root/common/*.c" | ForEach-Object FullName)
foreach ($driver in @('clk','gpio','sys','pwr','usbdev','uart1')) {
    $sources += "$SdkBase/SRC/StdPeriphDriver/CH58x_$driver.c"
}
$sources += "$SdkBase/SRC/RVMSIS/core_riscv.c","$SdkBase/SRC/Startup/startup_CH583.S"
$objects = @()
foreach ($source in $sources) {
    $obj = Join-Path $build ([IO.Path]::GetFileNameWithoutExtension($source) + '.o')
    & $cc @flags -c $source -o $obj
    if ($LASTEXITCODE -ne 0) { throw "Compile failed: $source" }
    $objects += $obj
}
$elf = Join-Path $out 'switchpro-CH582F-r1.elf'
$link = $cpu + $objects + @("$SdkBase/SRC/StdPeriphDriver/libISP583.a", "$SdkBase/BLE/LIB/libCH58xBLE.a",
    '-lc','-lm','-lnosys','-nostartfiles','-T',$ld,'-Wl,--gc-sections',"-Wl,-Map=$out/switchpro-CH582F-r1.map",
    '--specs=nano.specs','-o',$elf)
& $cc @link
if ($LASTEXITCODE -ne 0) { & $cc @link '-Wl,--no-relax' }
if ($LASTEXITCODE -ne 0) { throw 'Link failed' }
foreach ($format in @('binary','ihex')) {
    $ext = if ($format -eq 'binary') { 'bin' } else { 'hex' }
    & ($prefix + 'objcopy.exe') -O $format -S $elf "$out/switchpro-CH582F-r1.$ext"
    if ($LASTEXITCODE -ne 0) { throw 'Image conversion failed' }
}
[ordered]@{chip='CH582F'; input='ESP32 UART'; usbMode='xinput'; revision=1; uartRemap=$UartRemap;
    uartBaud=115200; writesDataFlash=$false; binarySha256=(Get-FileHash "$out/switchpro-CH582F-r1.bin").Hash;
    sourceHashes=@($sources | ForEach-Object { @{path=$_; sha256=(Get-FileHash $_).Hash} })
} | ConvertTo-Json -Depth 5 | Set-Content "$out/build-info.json" -Encoding utf8
& ($prefix + 'size.exe') $elf
if ($LASTEXITCODE -ne 0) { throw 'Size check failed' }
Write-Output "Built: $out/switchpro-CH582F-r1.bin"
