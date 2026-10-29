# SPDX-License-Identifier: Apache-2.0
param([ValidateSet('xinput','switch')][string]$UsbMode='xinput', [string]$StageDirectory='')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$name=if($UsbMode -eq 'switch') {'pico2w-switch-r1-1'} else {'pico2w-xinput-r5-1'}
foreach($version in @('r1','r2','r3','r4','r4-1','r4-2','r5')) { & "$PSScriptRoot/verify-$version.ps1" }
$info=Get-Content "$root/firmware/$name-build-info.json" -Raw | ConvertFrom-Json
foreach($record in $info.sourceHashes) {
    if((Get-FileHash -LiteralPath (Join-Path $root $record.path) -Algorithm SHA256).Hash -ne $record.sha256) {
        throw "Rebuild firmware after source change: $($record.path)"
    }
}
foreach($record in $info.outputs) {
    if((Get-FileHash -LiteralPath (Join-Path "$root/firmware" $record.file) -Algorithm SHA256).Hash -ne $record.sha256) {
        throw "Firmware output hash mismatch: $($record.file)"
    }
}
if(!$StageDirectory) {
    $purpose='package-'+$name+'-'+[guid]::NewGuid().ToString('N')
    if(Test-Path 'D:/Dev/Tools/Get-DevBuildPath.ps1') {
        $StageDirectory=& 'D:/Dev/Tools/Get-DevBuildPath.ps1' -ProjectPath (Split-Path $root -Parent) -Purpose $purpose
    } else { $StageDirectory=Join-Path $root "build/$purpose" }
}
$project=Join-Path $StageDirectory 'pico2w'
if(Test-Path $project) { throw "Package stage must be new: $project" }
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
foreach($ext in @('uf2','bin','elf','json')) {
    $file=if($ext -eq 'json') {"$name-build-info.json"} else {"$name.$ext"}
    Copy-Item -LiteralPath (Join-Path "$root/firmware" $file) -Destination $firmwareOut
}
$files=@(Get-ChildItem -LiteralPath $project -File -Recurse | ForEach-Object {
    [PSCustomObject]@{path=$_.FullName.Substring($project.Length+1).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
})
$files | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $project 'MANIFEST.json') -Encoding utf8
New-Item -ItemType Directory -Path (Join-Path $root 'releases') -Force | Out-Null
$zip=Join-Path $root "releases/$name-package.zip"
Compress-Archive -LiteralPath $project -DestinationPath $zip -Force
Get-FileHash -LiteralPath $zip -Algorithm SHA256 | Format-List
