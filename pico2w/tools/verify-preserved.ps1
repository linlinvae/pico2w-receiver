$ErrorActionPreference='Stop'
$root=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$files=Get-Content "$PSScriptRoot/../preserved/previous-firmware.json" -Raw | ConvertFrom-Json
foreach($f in $files) {
    $path=Join-Path $root $f.path
    if (!(Test-Path -LiteralPath $path) -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $f.sha256) {
        throw "Previous project changed: $($f.path)"
    }
}
Write-Output "Previous Stadia / Switch Pro projects preserved: $($files.Count) hashes verified."
