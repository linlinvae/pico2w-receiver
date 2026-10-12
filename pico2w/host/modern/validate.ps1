# SPDX-License-Identifier: Apache-2.0
param([switch]$Build,[string]$Name='delivery',[switch]$SourceCheck)
$ErrorActionPreference='Stop'
if($Name -notmatch '^[a-z0-9-]+$'){throw 'Invalid validation folder name'}
if($SourceCheck) {
    $hostRoot=Split-Path $PSScriptRoot -Parent
    $check=Join-Path $PSScriptRoot ('validation/source-check-'+[guid]::NewGuid().ToString('N'))
    Expand-Archive -LiteralPath (Join-Path $hostRoot 'releases/PicoControl-v0.3-source.zip') -DestinationPath $check
    $source=Join-Path $check 'PicoControl-source'
    $records=Get-Content -LiteralPath (Join-Path $source 'MANIFEST.json') -Raw | ConvertFrom-Json
    foreach($record in $records){if((Get-FileHash -LiteralPath (Join-Path $source $record.path) -Algorithm SHA256).Hash -ne $record.sha256){throw ('Source package mismatch: '+$record.path)}}
    Write-Output ('Source manifest verified: '+$records.Count+' files')
    Push-Location (Join-Path $source 'modern/ui')
    try {
        $pnpm=Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/pnpm/bin/pnpm.cjs'
        if(Test-Path -LiteralPath $pnpm){& node $pnpm install --offline --frozen-lockfile}else{& pnpm install --offline --frozen-lockfile}
        if($LASTEXITCODE -ne 0){throw 'Offline source restore failed'}
    } finally {Pop-Location}
    & (Join-Path $source 'modern/build.ps1') -Test
    Write-Output 'Standalone source package built and tested.'
    return
}
if($Build){& (Join-Path $PSScriptRoot 'build.ps1') -Test}
$app=Join-Path (Split-Path $PSScriptRoot -Parent) 'dist-v0.3/PicoControl.exe'
foreach($size in @('normal','small')) {
    $output=Join-Path $PSScriptRoot ('validation/'+$Name+'/'+$size)
    $arguments=@('--preview',('"'+$output+'"'))
    if($size -eq 'small'){$arguments+='small'}
    $p=Start-Process -FilePath $app -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
    Write-Output ($size+' exit '+$p.ExitCode)
    Get-Content -LiteralPath (Join-Path $output 'ui-checks.json')
    if($p.ExitCode -ne 0){throw ('Preview failed: '+$size)}
}
