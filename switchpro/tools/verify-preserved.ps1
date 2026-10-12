$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$manifest = Get-Content "$PSScriptRoot/../preserved/receiver-manifest.json" -Raw | ConvertFrom-Json
foreach ($file in $manifest) {
    $path = Join-Path $root $file.path
    if (!(Test-Path -LiteralPath $path) -or (Get-FileHash -LiteralPath $path).Hash -ne $file.sha256) {
        throw "Original receiver file changed: $($file.path)"
    }
}
Write-Output "Original Stadia receiver unchanged: $($manifest.Count) file hashes verified."
