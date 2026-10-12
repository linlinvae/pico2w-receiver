# SPDX-License-Identifier: Apache-2.0
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$files=Get-Content "$root/preserved/r5-artifacts.json" -Raw | ConvertFrom-Json
foreach($f in $files) {
    $path=Join-Path $root $f.path
    if(!(Test-Path -LiteralPath $path)) { continue }
    if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $f.sha256) {
        throw "R5 artifact changed: $($f.path)"
    }
}
Write-Output 'Existing R5 firmware and V0.2 release hashes verified.'
