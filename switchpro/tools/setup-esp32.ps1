param([string]$Python = 'python')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$lock = Get-Content "$root/dependencies.lock.json" -Raw | ConvertFrom-Json
New-Item -ItemType Directory -Force -Path "$root/resources","$root/resources/tmp" | Out-Null
$env:TEMP = (Resolve-Path "$root/resources/tmp").Path
$env:TMP = $env:TEMP
$env:IDF_TOOLS_PATH = Join-Path $root 'resources/idf-tools'
function Get-PinnedRepository($path, $dependency) {
    if (!(Test-Path -LiteralPath "$path/.git")) {
        & git -c credential.helper= clone --no-checkout $dependency.url $path
        if ($LASTEXITCODE -ne 0) { throw "Download failed: $($dependency.url)" }
        & git -C $path checkout --detach $dependency.commit
        if ($LASTEXITCODE -ne 0) { throw "Checkout failed: $path" }
    }
    $head = & git -C $path rev-parse HEAD
    if ($LASTEXITCODE -ne 0 -or $head -ne $dependency.commit) { throw "Unexpected dependency revision: $path" }
    $changes = & git -C $path status --porcelain --untracked-files=no
    if ($LASTEXITCODE -ne 0 -or $changes) { throw "Dependency has changes; preserved: $path" }
}
Get-PinnedRepository "$root/resources/bluepad32" $lock.bluepad32
Get-PinnedRepository "$root/resources/bluepad32/external/btstack" $lock.btstack
Get-PinnedRepository "$root/resources/esp-idf" $lock.espIdf
$idf = Join-Path $root 'resources/esp-idf'
# No BTstack security-relaxation patch is required for the Switch Pro target.
$submodules = @('components/bt/controller/lib_esp32','components/mbedtls/mbedtls',
    'components/esp_wifi/lib','components/esp_phy/lib','components/esp_coex/lib',
    'components/lwip/lwip','components/json/cJSON','components/spiffs/spiffs',
    'components/bootloader/subproject/components/micro-ecc/micro-ecc',
    'components/mqtt/esp-mqtt','components/heap/tlsf','components/protobuf-c/protobuf-c',
    'components/unity/unity')
foreach ($submodule in $submodules) {
    $path = Join-Path $idf $submodule
    if (Test-Path -LiteralPath "$path/.git") {
        $dirty = & git -C $path status --porcelain --untracked-files=no
        if ($LASTEXITCODE -ne 0 -or $dirty) { throw "Changed dependency preserved: $path" }
    }
}
& git -c credential.helper= -C $idf submodule update --init --recursive --depth 1 @submodules
if ($LASTEXITCODE -ne 0) { throw 'ESP-IDF dependency download failed' }
& $Python "$idf/tools/idf_tools.py" --non-interactive install xtensa-esp-elf esp32ulp-elf ninja cmake
if ($LASTEXITCODE -ne 0) { throw 'ESP32 toolchain setup failed' }
& $Python "$idf/tools/idf_tools.py" --non-interactive install-python-env
if ($LASTEXITCODE -ne 0) { throw 'ESP-IDF Python setup failed (use Python 3.12)' }
Write-Output 'ESP32 dependencies ready; run tools/build-esp32.ps1.'
