# SPDX-License-Identifier: Apache-2.0
param([switch]$Test,[switch]$SkipUi)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$pnpm=Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/pnpm/bin/pnpm.cjs'
if(!$SkipUi) {
    Push-Location (Join-Path $PSScriptRoot 'ui')
    try {if(Test-Path -LiteralPath $pnpm){& node $pnpm run build}else{& pnpm run build};if($LASTEXITCODE -ne 0){throw 'UI build failed'}}finally{Pop-Location}
}
$out=Join-Path $root 'dist-v0.3'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$sdk=Join-Path $PSScriptRoot 'vendor/webview2'
$compiler=Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
$sources=@(Get-ChildItem -LiteralPath (Join-Path $root 'src') -Filter '*.cs' | ForEach-Object FullName)+@(Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'native') -Filter '*.cs' | ForEach-Object FullName)
$exe=Join-Path $out 'PicoControl.exe'
& $compiler /nologo /target:winexe /platform:x64 /optimize+ /utf8output /main:PicoControl.ModernProgram "/out:$exe" "/resource:$root/validation/config-vectors.bin,PicoControl.ConfigVectors" /reference:System.dll /reference:System.Core.dll /reference:System.Drawing.dll /reference:System.Windows.Forms.dll /reference:System.Web.Extensions.dll "/reference:$sdk/lib/net462/Microsoft.Web.WebView2.Core.dll" "/reference:$sdk/lib/net462/Microsoft.Web.WebView2.WinForms.dll" $sources
if($LASTEXITCODE -ne 0){throw 'Desktop build failed'}
foreach($dll in @('Microsoft.Web.WebView2.Core.dll','Microsoft.Web.WebView2.WinForms.dll')){Copy-Item -LiteralPath (Join-Path $sdk ('lib/net462/'+$dll)) -Destination $out -Force}
Copy-Item -LiteralPath (Join-Path $sdk 'runtimes/win-x64/native/WebView2Loader.dll') -Destination $out -Force
New-Item -ItemType Directory -Path (Join-Path $out 'ui') -Force | Out-Null
Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'ui/dist') | Copy-Item -Destination (Join-Path $out 'ui') -Recurse -Force
if(Test-Path -LiteralPath (Join-Path $PSScriptRoot 'README.md')){Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'README.md') -Destination $out -Force}
$license=Join-Path $root 'LICENSE'
if(!(Test-Path -LiteralPath $license)){$license=Join-Path $root '../LICENSE'}
Copy-Item -LiteralPath $license -Destination $out -Force
foreach($document in @('VALIDATION.md','THIRD-PARTY-NOTICES.txt')) {if(Test-Path -LiteralPath (Join-Path $PSScriptRoot $document)){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $document) -Destination $out -Force}}
Copy-Item -LiteralPath (Join-Path $sdk 'LICENSE.txt') -Destination (Join-Path $out 'WEBVIEW2-SDK-LICENSE.txt') -Force
if($Test){$report=Join-Path $out 'self-test.txt';$p=Start-Process -FilePath $exe -ArgumentList @('--self-test',('"'+$report+'"')) -WindowStyle Hidden -Wait -PassThru;if($p.ExitCode -ne 0){Get-Content -LiteralPath $report;throw 'Desktop self-test failed'};Get-Content -LiteralPath $report}
Get-FileHash -LiteralPath $exe -Algorithm SHA256 | Format-List
