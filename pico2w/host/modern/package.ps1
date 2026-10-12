# SPDX-License-Identifier: Apache-2.0
$ErrorActionPreference='Stop'
$hostRoot=Split-Path $PSScriptRoot -Parent
$out=Join-Path $hostRoot 'dist-v0.3'
$stage=Join-Path $PSScriptRoot ('package-stage/'+[guid]::NewGuid().ToString('N'))
$runtime=Join-Path $stage 'PicoControl'
New-Item -ItemType Directory -Path (Join-Path $runtime 'ui/assets') -Force | Out-Null
foreach($file in @('PicoControl.exe','Microsoft.Web.WebView2.Core.dll','Microsoft.Web.WebView2.WinForms.dll','WebView2Loader.dll','self-test.txt')){Copy-Item -LiteralPath (Join-Path $out $file) -Destination $runtime}
foreach($file in @('README.md','VALIDATION.md','THIRD-PARTY-NOTICES.txt')){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination $runtime;Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination $out -Force}
$license=Join-Path $hostRoot 'LICENSE'
if(!(Test-Path -LiteralPath $license)){$license=Join-Path $hostRoot '../LICENSE'}
Copy-Item -LiteralPath $license -Destination $runtime
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'vendor/webview2/LICENSE.txt') -Destination (Join-Path $runtime 'WEBVIEW2-SDK-LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $out 'ui/index.html') -Destination (Join-Path $runtime 'ui')
$html=Get-Content -LiteralPath (Join-Path $out 'ui/index.html') -Raw
foreach($match in [regex]::Matches($html,'(?:src|href)="\.\/assets\/([^"\s]+)"')){Copy-Item -LiteralPath (Join-Path $out ('ui/assets/'+$match.Groups[1].Value)) -Destination (Join-Path $runtime 'ui/assets')}
$source=Join-Path $stage 'PicoControl-source'
New-Item -ItemType Directory -Path (Join-Path $source 'modern'),(Join-Path $source 'validation') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $hostRoot 'src') -Destination $source -Recurse
Copy-Item -LiteralPath (Join-Path $hostRoot 'validation/config-vectors.bin') -Destination (Join-Path $source 'validation')
Copy-Item -LiteralPath $license -Destination $source
$modern=Join-Path $source 'modern'
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'native') -Destination $modern -Recurse
foreach($file in @('README.md','VALIDATION.md','THIRD-PARTY-NOTICES.txt','build.ps1','validate.ps1','package.ps1','build-ui-sandbox.ps1')){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination $modern}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'README.md') -Destination $source
$ui=Join-Path $modern 'ui'
New-Item -ItemType Directory -Path $ui -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'ui/src') -Destination $ui -Recurse
foreach($file in @('package.json','pnpm-lock.yaml','pnpm-workspace.yaml','components.json','tsconfig.json','vite.config.ts','index.html')){Copy-Item -LiteralPath (Join-Path $PSScriptRoot ('ui/'+$file)) -Destination $ui}
$sdk=Join-Path $modern 'vendor/webview2'
New-Item -ItemType Directory -Path (Join-Path $sdk 'lib/net462'),(Join-Path $sdk 'runtimes/win-x64/native') -Force | Out-Null
foreach($file in @('Microsoft.Web.WebView2.Core.dll','Microsoft.Web.WebView2.WinForms.dll')){Copy-Item -LiteralPath (Join-Path $PSScriptRoot ('vendor/webview2/lib/net462/'+$file)) -Destination (Join-Path $sdk 'lib/net462')}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'vendor/webview2/runtimes/win-x64/native/WebView2Loader.dll') -Destination (Join-Path $sdk 'runtimes/win-x64/native')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'vendor/webview2/LICENSE.txt') -Destination $sdk
foreach($folder in @($runtime,$source)) {
    $records=@(Get-ChildItem -LiteralPath $folder -Recurse -File | ForEach-Object {[PSCustomObject]@{path=$_.FullName.Substring($folder.Length+1).Replace('\','/');sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}})
    $records | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $folder 'MANIFEST.json') -Encoding utf8
}
$release=Join-Path $hostRoot 'releases'
Compress-Archive -LiteralPath $runtime -DestinationPath (Join-Path $release 'PicoControl-v0.3-windows.zip') -Force
Compress-Archive -LiteralPath $source -DestinationPath (Join-Path $release 'PicoControl-v0.3-source.zip') -Force
Get-FileHash -LiteralPath (Join-Path $release 'PicoControl-v0.3-windows.zip'),(Join-Path $release 'PicoControl-v0.3-source.zip') -Algorithm SHA256 | Format-List
