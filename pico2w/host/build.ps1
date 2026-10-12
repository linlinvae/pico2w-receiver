# SPDX-License-Identifier: Apache-2.0
param([switch]$Test)
$ErrorActionPreference='Stop'
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (!(Test-Path -LiteralPath $compiler)) { $compiler=Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319\csc.exe' }
if (!(Test-Path -LiteralPath $compiler)) { throw '未找到 Windows .NET Framework 编译器。' }
$output=Join-Path $PSScriptRoot 'dist-v0.2'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$exe=Join-Path $output 'PicoControl.exe'
$sources=Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'src') -Filter '*.cs' | ForEach-Object { $_.FullName }
& $compiler /nologo /target:winexe /platform:anycpu /optimize+ /utf8output "/out:$exe" "/resource:$PSScriptRoot/validation/config-vectors.bin,PicoControl.ConfigVectors" /reference:System.dll /reference:System.Core.dll /reference:System.Drawing.dll /reference:System.Windows.Forms.dll $sources
if ($LASTEXITCODE -ne 0) { throw '上位机构建失败。' }
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'README.md') -Destination (Join-Path $output 'README.md') -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'VALIDATION.md') -Destination (Join-Path $output 'VALIDATION.md') -Force
$license=Join-Path $PSScriptRoot 'LICENSE'
if(!(Test-Path -LiteralPath $license)) { $license=Join-Path $PSScriptRoot '..\LICENSE' }
Copy-Item -LiteralPath $license -Destination (Join-Path $output 'LICENSE') -Force
$protocol=Join-Path $PSScriptRoot '..\docs\config-protocol.md'
if(!(Test-Path -LiteralPath $protocol)) { $protocol=Join-Path $PSScriptRoot 'docs\config-protocol.md' }
if(Test-Path -LiteralPath $protocol) { Copy-Item -LiteralPath $protocol -Destination (Join-Path $output 'CONFIG-PROTOCOL.md') -Force }
if($Test) {
    $report=Join-Path $output 'self-test.txt'
    $process=Start-Process -FilePath $exe -ArgumentList @('--self-test',('"'+$report+'"')) -WindowStyle Hidden -Wait -PassThru
    if($process.ExitCode -ne 0) { if(Test-Path -LiteralPath $report){Get-Content -LiteralPath $report}; throw '自测失败。' }
    Get-Content -LiteralPath $report
}
Get-FileHash -LiteralPath $exe -Algorithm SHA256 | Select-Object Path,Hash
