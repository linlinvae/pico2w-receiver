# SPDX-License-Identifier: Apache-2.0
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$hostRoot=Join-Path $root 'host'
& "$PSScriptRoot/verify-r4-2.ps1"
$stage=Join-Path $root ('build/package-r5-'+[guid]::NewGuid().ToString('N'))
$project=Join-Path $stage 'pico2w'
New-Item -ItemType Directory -Path $project -Force | Out-Null
foreach($folder in @('src','docs','tools','preserved')) {Copy-Item -LiteralPath (Join-Path $root $folder) -Destination $project -Recurse}
foreach($file in @('CMakeLists.txt','flash_guard.ld','dependencies.lock.json','README.md','VALIDATION.md','CHANGELOG.md','LICENSE')) {Copy-Item -LiteralPath (Join-Path $root $file) -Destination $project}
$testOut=Join-Path $project 'tests'
New-Item -ItemType Directory -Path $testOut -Force | Out-Null
Get-ChildItem -LiteralPath (Join-Path $root 'tests') -File | Copy-Item -Destination $testOut
Copy-Item -LiteralPath (Join-Path $root 'tests/stubs') -Destination $testOut -Recurse
$firmwareOut=Join-Path $project 'firmware'
New-Item -ItemType Directory -Path $firmwareOut -Force | Out-Null
Get-ChildItem -LiteralPath (Join-Path $root 'firmware') -Filter 'pico2w-xinput-r5*' -File | Copy-Item -Destination $firmwareOut
if(!(Test-Path (Join-Path $firmwareOut 'pico2w-xinput-r5.uf2'))) {throw 'Build R5 firmware first'}
$hostOut=Join-Path $project 'host'
New-Item -ItemType Directory -Path (Join-Path $hostOut 'validation') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $hostRoot 'src') -Destination $hostOut -Recurse
foreach($file in @('build.ps1','README.md','VALIDATION.md')) {Copy-Item -LiteralPath (Join-Path $hostRoot $file) -Destination $hostOut}
Copy-Item -LiteralPath (Join-Path $hostRoot 'validation/config-vectors.bin') -Destination (Join-Path $hostOut 'validation')
Copy-Item -LiteralPath (Join-Path $hostRoot 'dist-v0.2') -Destination $hostOut -Recurse
$files=@(Get-ChildItem -LiteralPath $project -File -Recurse | ForEach-Object {[PSCustomObject]@{path=$_.FullName.Substring($project.Length+1).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}})
$files | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $project 'MANIFEST.json') -Encoding utf8
$releases=Join-Path $root 'releases'
New-Item -ItemType Directory -Path $releases -Force | Out-Null
Compress-Archive -LiteralPath $project -DestinationPath (Join-Path $releases 'pico2w-xinput-r5-package.zip') -Force
$hostReleases=Join-Path $hostRoot 'releases'
New-Item -ItemType Directory -Path $hostReleases -Force | Out-Null
$hostFiles=Get-ChildItem -LiteralPath (Join-Path $hostRoot 'dist-v0.2') -File | ForEach-Object FullName
Compress-Archive -LiteralPath $hostFiles -DestinationPath (Join-Path $hostReleases 'PicoControl-v0.2-windows.zip') -Force
$standalone=Join-Path $stage 'PicoControl-source'
New-Item -ItemType Directory -Path (Join-Path $standalone 'validation'),(Join-Path $standalone 'docs') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $hostRoot 'src') -Destination $standalone -Recurse
foreach($file in @('build.ps1','README.md','VALIDATION.md')) {Copy-Item -LiteralPath (Join-Path $hostRoot $file) -Destination $standalone}
Copy-Item -LiteralPath (Join-Path $root 'LICENSE') -Destination $standalone
Copy-Item -LiteralPath (Join-Path $root 'docs/config-protocol.md') -Destination (Join-Path $standalone 'docs')
Copy-Item -LiteralPath (Join-Path $hostRoot 'validation/config-vectors.bin') -Destination (Join-Path $standalone 'validation')
Compress-Archive -LiteralPath $standalone -DestinationPath (Join-Path $hostReleases 'PicoControl-v0.2-source.zip') -Force
Get-FileHash -LiteralPath (Join-Path $releases 'pico2w-xinput-r5-package.zip'),(Join-Path $hostReleases 'PicoControl-v0.2-windows.zip'),(Join-Path $hostReleases 'PicoControl-v0.2-source.zip') -Algorithm SHA256 | Format-List
