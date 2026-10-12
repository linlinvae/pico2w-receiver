$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$lock=Get-Content "$root/dependencies.lock.json" -Raw | ConvertFrom-Json
New-Item -ItemType Directory -Force "$root/resources","$root/resources/downloads" | Out-Null
function Get-Pinned($path,$dep) {
    if (!(Test-Path -LiteralPath "$path/.git")) {
        & git -c credential.helper= clone --no-checkout $dep.url $path
        if ($LASTEXITCODE -ne 0) {throw "Clone failed: $($dep.url)"}
        & git -C $path checkout --detach $dep.commit
        if ($LASTEXITCODE -ne 0) {throw "Checkout failed: $path"}
    }
    $absolute=(Resolve-Path $path).Path
    $head=& git -c "safe.directory=$absolute" -C $path rev-parse HEAD
    if ($LASTEXITCODE -ne 0 -or $head -ne $dep.commit) {throw "Changed revision preserved: $path"}
    $dirty=& git -c "safe.directory=$absolute" -C $path status --porcelain --untracked-files=no
    if ($LASTEXITCODE -ne 0 -or $dirty) {throw "Changed dependency preserved: $path"}
}
Get-Pinned "$root/resources/pico-sdk" $lock.picoSdk
Get-Pinned "$root/resources/bluepad32" $lock.bluepad32
Get-Pinned "$root/resources/bluepad32/external/btstack" $lock.btstack
$sdk=(Resolve-Path "$root/resources/pico-sdk").Path
foreach($p in $lock.submodules.PSObject.Properties) {
    $sub=Join-Path $sdk $p.Name
    if(Test-Path "$sub/.git") {
        $absolute=(Resolve-Path $sub).Path
        $dirty=& git -c "safe.directory=$absolute" -C $sub status --porcelain --untracked-files=no
        if ($LASTEXITCODE -ne 0 -or $dirty) {throw "Changed submodule preserved: $sub"}
    }
    & git -c "safe.directory=$sdk" -c credential.helper= -C $sdk submodule update --init --depth 1 $p.Name
    if ($LASTEXITCODE -ne 0) {throw "Submodule download failed: $($p.Name)"}
}
foreach($p in $lock.tools.PSObject.Properties) {
    $tool=$p.Value; $dest=Join-Path "$root/resources" $tool.folder
    if (Test-Path (Join-Path $dest $tool.check)) {continue}
    $archive=Join-Path "$root/resources/downloads" $tool.archive
    if (!(Test-Path $archive)) {Invoke-WebRequest $tool.url -OutFile $archive}
    if((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $tool.sha256) {
        throw "Download checksum mismatch: $archive"
    }
    if(Test-Path $dest) {throw "Partial tool directory preserved; inspect before retrying: $dest"}
    Expand-Archive -LiteralPath $archive -DestinationPath $dest
}
& "$PSScriptRoot/verify-dependencies.ps1"
Write-Output 'Dependencies ready. Run tools/build.ps1 with your Python 3 executable.'
