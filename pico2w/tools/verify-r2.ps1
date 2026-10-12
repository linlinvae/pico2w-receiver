$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$files=Get-Content "$root/preserved/r2-artifacts.json" -Raw | ConvertFrom-Json
foreach($f in $files) {
    $path=Join-Path $root $f.path
    if (!(Test-Path -LiteralPath $path)) { continue } # Optional in an R3-only package.
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $f.sha256) {
        throw "R2 artifact changed: $($f.path)"
    }
}
Write-Output 'Existing R2 firmware/package hashes verified.'
