$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$files=Get-Content "$root/preserved/r3-artifacts.json" -Raw | ConvertFrom-Json
foreach($f in $files) {
    $path=Join-Path $root $f.path
    if (!(Test-Path -LiteralPath $path)) { continue } # Optional in an R4-only package.
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $f.sha256) {
        throw "R3 artifact changed: $($f.path)"
    }
}
Write-Output 'Existing R3 firmware/package hashes verified.'

