$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$lock=Get-Content "$root/dependencies.lock.json" -Raw | ConvertFrom-Json
function Assert-Revision($path,$expected) {
    $path=(Resolve-Path -LiteralPath $path).Path.Replace('\','/')
    $actual=& git -c "safe.directory=$path" -C $path rev-parse HEAD
    if ($LASTEXITCODE -ne 0 -or $actual -ne $expected) {throw "Unexpected dependency revision: $path"}
    $dirty=& git -c "safe.directory=$path" -C $path status --porcelain --untracked-files=no
    if ($LASTEXITCODE -ne 0 -or $dirty) {throw "Dependency modified; preserved: $path"}
}
Assert-Revision "$root/resources/pico-sdk" $lock.picoSdk.commit
Assert-Revision "$root/resources/bluepad32" $lock.bluepad32.commit
Assert-Revision "$root/resources/bluepad32/external/btstack" $lock.btstack.commit
foreach($p in $lock.submodules.PSObject.Properties) {
    Assert-Revision "$root/resources/pico-sdk/$($p.Name)" $p.Value
}
Write-Output 'Six pinned source dependencies verified; no tracked modifications.'
