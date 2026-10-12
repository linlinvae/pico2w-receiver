# SPDX-License-Identifier: Apache-2.0
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
& "$PSScriptRoot/verify-r5.ps1"
$stage=Join-Path $root ('build/package-switch-r1-'+[guid]::NewGuid().ToString('N'))
$project=Join-Path $stage 'pico2w'
New-Item -ItemType Directory -Path $project -Force | Out-Null
foreach($folder in @('src','docs','tools','preserved','licenses')) {
    Copy-Item -LiteralPath (Join-Path $root $folder) -Destination $project -Recurse
}
foreach($file in @('CMakeLists.txt','flash_guard.ld','dependencies.lock.json','README.md','VALIDATION.md','CHANGELOG.md','LICENSE','NOTICE.md')) {
    Copy-Item -LiteralPath (Join-Path $root $file) -Destination $project
}
$testOut=Join-Path $project 'tests'
New-Item -ItemType Directory -Path $testOut -Force | Out-Null
Get-ChildItem -LiteralPath (Join-Path $root 'tests') -File | Copy-Item -Destination $testOut
Copy-Item -LiteralPath (Join-Path $root 'tests/stubs') -Destination $testOut -Recurse
$firmwareOut=Join-Path $project 'firmware'
New-Item -ItemType Directory -Path $firmwareOut -Force | Out-Null
Get-ChildItem -LiteralPath (Join-Path $root 'firmware') -Filter 'pico2w-switch-r1*' -File | Copy-Item -Destination $firmwareOut
if(!(Test-Path (Join-Path $firmwareOut 'pico2w-switch-r1.uf2'))) { throw 'Build Switch R1 first' }
$files=@(Get-ChildItem -LiteralPath $project -File -Recurse | ForEach-Object {
    [PSCustomObject]@{path=$_.FullName.Substring($project.Length+1).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
})
$files | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $project 'MANIFEST.json') -Encoding utf8
$releases=Join-Path $root 'releases'
New-Item -ItemType Directory -Path $releases -Force | Out-Null
$zip=Join-Path $releases 'pico2w-switch-r1-package.zip'
Compress-Archive -LiteralPath $project -DestinationPath $zip -Force
Get-FileHash -LiteralPath $zip -Algorithm SHA256 | Format-List
