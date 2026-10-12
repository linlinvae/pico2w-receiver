param(
    [Parameter(Mandatory=$true)][ValidatePattern('^COM\d+$')][string]$Port,
    [ValidateRange(1,60)][int]$Seconds = 45,
    [string]$OutputPath
)
$ErrorActionPreference = 'Stop'
if (!$OutputPath) {
    $projectRoot = Split-Path $PSScriptRoot -Parent
    $logDir = Join-Path $projectRoot 'build/hardware-logs'
    New-Item -ItemType Directory -Path $logDir -Force | Out-Null
    $OutputPath = Join-Path $logDir ('capture-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.txt')
}
$OutputPath = [IO.Path]::GetFullPath($OutputPath)
New-Item -ItemType Directory -Path (Split-Path $OutputPath -Parent) -Force | Out-Null
$writer = New-Object IO.StreamWriter($OutputPath, $false, [Text.UTF8Encoding]::new($false))
$timer = [Diagnostics.Stopwatch]::StartNew()
$serialPort = $null
$lastError = ''
try {
    Write-Host "Reading $Port for $Seconds seconds; log: $OutputPath"
    while ($timer.Elapsed.TotalSeconds -lt $Seconds) {
        try {
            if (!$serialPort -or !$serialPort.IsOpen) {
                if ($serialPort) { $serialPort.Dispose() }
                $serialPort = New-Object IO.Ports.SerialPort($Port,115200,'None',8,'One')
                $serialPort.ReadTimeout = 100
                $serialPort.DtrEnable = $true
                $serialPort.Open()
                $lastError = ''
                Write-Host "Opened $Port (DTR enabled)"
            }
            $chunk = $serialPort.ReadExisting()
            if ($chunk) {
                $writer.Write($chunk)
                $writer.Flush()
                Write-Host -NoNewline $chunk
            } else {
                Start-Sleep -Milliseconds 50
            }
        } catch {
            $errorText = $_.Exception.Message
            if ($errorText -ne $lastError) {
                Write-Host "Port unavailable: $errorText"
                $writer.WriteLine("[CAPTURE] $errorText")
                $writer.Flush()
                $lastError = $errorText
            }
            if ($serialPort) { $serialPort.Dispose(); $serialPort = $null }
            Start-Sleep -Milliseconds 500
        }
    }
} finally {
    if ($serialPort) { $serialPort.Dispose() }
    $writer.Dispose()
}
Write-Host "`nSaved: $OutputPath"
