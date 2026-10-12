param(
    [ValidateSet('CH592','CH582')][string]$Chip = 'CH582',
    [ValidateSet('dev','custom')][string]$Board = 'dev',
    [ValidateSet('standard','scan-diag')][string]$Variant = 'standard',
    [ValidateSet('sinput','xinput')][string]$UsbMode = 'sinput',
    [string]$ToolchainBin,
    [string]$SdkBase,
    [ValidateSet(0,1,2,3,4)][int]$LogLevel = 3,
    [ValidateRange(2048,8192)][int]$StackSize = 2048,
    [string[]]$BoardDefines = @()
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (!$ToolchainBin) { $ToolchainBin = Join-Path $projectRoot 'resources/wch-gcc/bin' }
$prefix = Join-Path $ToolchainBin 'riscv-none-embed-'
$compiler = $prefix + 'gcc.exe'
if (!(Test-Path -LiteralPath $compiler)) {
    throw "WCH compiler missing. Set -ToolchainBin to the MounRiver RISC-V Embedded GCC bin directory."
}
if (!$SdkBase) {
    $sdkName = if ($Chip -eq 'CH592') { 'CH592EVT' } else { 'CH583EVT' }
    $SdkBase = Join-Path $projectRoot "sdk/$sdkName/EVT/EXAM"
}
$family = if ($Chip -eq 'CH592') { 'CH59x' } else { 'CH58x' }
$sdkChip = if ($Chip -eq 'CH592') { '592' } else { '583' }
$libraries = @("$SdkBase/SRC/StdPeriphDriver/libISP$sdkChip.a", "$SdkBase/BLE/LIB/lib${family}BLE.a")
foreach ($required in $libraries + @("$SdkBase/SRC/Ld/Link.ld")) {
    if (!(Test-Path -LiteralPath $required)) { throw "SDK file missing: $required" }
}
$buildLabel = "$Chip-$Board"
if ($UsbMode -eq 'xinput') { $buildLabel += '-xinput' }
if ($Variant -ne 'standard') { $buildLabel += "-$Variant" }
$buildDir = Join-Path $projectRoot "build/$buildLabel"
$firmwareDir = Join-Path $projectRoot "firmware/$buildLabel"
New-Item -ItemType Directory -Path $buildDir,$firmwareDir -Force | Out-Null
# Generate a linker script without modifying the SDK. Logging and USB snapshots
# need more stack than the SDK example's 512 bytes; the linker checks overlap.
$linkerScript = Join-Path $buildDir 'Link.ld'
$linkerText = [IO.File]::ReadAllText("$SdkBase/SRC/Ld/Link.ld")
if ($linkerText -notmatch '__stack_size\s*=\s*\d+\s*;') { throw 'Unsupported SDK linker script.' }
$linkerText = $linkerText -replace '__stack_size\s*=\s*\d+\s*;', "__stack_size = $StackSize;"
$linkerText += "`nASSERT(_ebss <= _susrstack, `"RAM overlaps the reserved stack`");`n"
[IO.File]::WriteAllText($linkerScript, $linkerText, [Text.UTF8Encoding]::new($false))
$cpu = @('-march=rv32imac','-mabi=ilp32','-mcmodel=medany','-msmall-data-limit=8','-mno-save-restore')
$flags = $cpu + @('-Os','-ffunction-sections','-fdata-sections','-g','-gdwarf-2',
    '-Wall','-Wextra','-Wno-unused-parameter','-Wno-unused-function','-std=gnu99',
    '-DBLE_MEMHEAP_SIZE=8192','-DBLE_BUFF_NUM=8','-DBLE_TX_NUM_EVENT=3',
    "-DCFG_$Chip","-DLOG_LEVEL=$LogLevel",
    '-include',"$projectRoot/src/board_config.h",
    "-I$projectRoot/src","-I$SdkBase/SRC/RVMSIS","-I$SdkBase/SRC/StdPeriphDriver/inc",
    "-I$SdkBase/BLE/HAL/include","-I$SdkBase/BLE/LIB")
$flags += if ($Chip -eq 'CH592') { @('-DISP592','-DCH59xBLE') } else { @('-DISP582','-DCH58xBLE') }
if ($Board -eq 'custom') { $flags += '-DBOARD_CUSTOM=1' }
if ($Variant -eq 'scan-diag') { $flags += '-DSTADIA_SCAN_DIAGNOSTICS=1' }
if ($UsbMode -eq 'xinput') { $flags += '-DUSB_GAMEPAD_XINPUT=1' }
foreach ($define in $BoardDefines) {
    if ($define -notmatch '^BOARD_[A-Z_]+=[A-Za-z0-9_]+$') { throw "Invalid board define: $define" }
    $flags += "-D$define"
}
# Record the effective board clock, including any command-line override.
$clockMacros = @('' | & $compiler @flags '-dM' '-E' '-x' 'c' '-')
if ($LASTEXITCODE -ne 0) { throw 'Clock configuration preprocessing failed.' }
$clockDefinition = $clockMacros | Where-Object { $_ -match '^#define BOARD_LF_CLOCK_SOURCE [012]$' }
if (!$clockDefinition) { throw 'BOARD_LF_CLOCK_SOURCE must be 0, 1 or 2.' }
$clockSource = [int](($clockDefinition -split ' ')[-1])
$sources = @(Get-ChildItem "$projectRoot/src/*.c" | ForEach-Object { $_.FullName })
foreach ($driver in @('clk','gpio','sys','pwr','usbdev','adc')) {
    $sources += "$SdkBase/SRC/StdPeriphDriver/${family}_$driver.c"
}
$sources += @("$SdkBase/BLE/HAL/MCU.c","$SdkBase/BLE/HAL/RTC.c",
    "$SdkBase/SRC/RVMSIS/core_riscv.c","$SdkBase/SRC/Startup/startup_CH$sdkChip.S")
if ($Chip -eq 'CH592') { $sources += "$SdkBase/BLE/LIB/ble_task_scheduler.S" }
$objects = @()
foreach ($source in $sources) {
    $object = Join-Path $buildDir ([IO.Path]::GetFileNameWithoutExtension($source) + '.o')
    Write-Host ('Compile ' + [IO.Path]::GetFileName($source))
    & $compiler @flags '-c' $source '-o' $object
    if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $source" }
    $objects += $object
}
$elf = Join-Path $firmwareDir 'stick.elf'
$linkArgs = $cpu + $objects + $libraries + @('-lc','-lm','-lnosys','-nostartfiles',
    '-T',$linkerScript,'-Wl,--gc-sections',"-Wl,-Map=$firmwareDir/stick.map",
    '--specs=nano.specs','-o',$elf)
$linkerRelaxation = $true
& $compiler @linkArgs
# The bundled Windows linker exited 5 on the R9 CH582 standard layout. A single
# retry without relaxation links the same objects. GCC reports that underlying
# linker exit as 1; any failure that persists on retry still stops the build.
if ($LASTEXITCODE -ne 0) {
    Write-Host 'WCH link failed; retrying once without linker relaxation.'
    $linkerRelaxation = $false
    & $compiler @linkArgs '-Wl,--no-relax'
}
if ($LASTEXITCODE -ne 0) { throw 'Link failed.' }
foreach ($format in @('binary','ihex')) {
    $ext = if ($format -eq 'binary') { 'bin' } else { 'hex' }
    & ($prefix + 'objcopy.exe') '-O' $format '-S' $elf "$firmwareDir/stick.$ext"
    if ($LASTEXITCODE -ne 0) { throw "objcopy failed: $format" }
}
$buildInfo = [ordered]@{
    firmwareRevision = [int](($clockMacros | Where-Object { $_ -match '^#define STADIA_SCAN_DIAG_REVISION [0-9]+$' } | ForEach-Object { ($_ -split ' ')[-1] }))
    chip = $Chip
    board = $Board
    variant = $Variant
    usbMode = $UsbMode
    logLevel = $LogLevel
    stackBytes = $StackSize
    linkerRelaxation = $linkerRelaxation
    rtcClockSource = $clockSource
    rtcClockHz = $(if ($clockSource -eq 1) { 32000 } else { 32768 })
    peerStoreDataFlashAddress = (($clockMacros | Where-Object { $_ -match '^#define BOARD_PEER_STORE_ADDR ' }) -split ' ')[-1]
    boardDefines = $BoardDefines
    compiler = (& $compiler '--version' | Select-Object -First 1)
    binarySha256 = (Get-FileHash "$firmwareDir/stick.bin" -Algorithm SHA256).Hash
    sourceHashes = @(Get-ChildItem "$projectRoot/src/*" -File | Sort-Object Name | ForEach-Object {
        @{ file = $_.Name; sha256 = (Get-FileHash $_.FullName -Algorithm SHA256).Hash }
    })
}
$buildInfo | ConvertTo-Json -Depth 5 | Set-Content "$firmwareDir/build-info.json" -Encoding UTF8
& ($prefix + 'size.exe') $elf
if ($LASTEXITCODE -ne 0) { throw 'Size check failed.' }
Write-Host "Built $Chip/$Board/$Variant : $firmwareDir/stick.bin"
